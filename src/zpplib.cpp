#include "zpplib.hpp"

#define windowBits 15
#define GZIP_ENCODING 16

namespace slx
{
  ZppRA::ZppRA(const std::string & i_filename)
  {
    Open(i_filename);
  }

  ZppRA::ZppRA(FILE * i_file)
  {
    Open(i_file);
  }

  ZppRA::~ZppRA()
  {
    Close();
  }

  int ZppRA::Open(const std::string & i_filename, bool i_build_index)
  {
    Close();

    m_cur_pos = 0;

    m_file = fopen(i_filename.c_str(), "rb");
    if (m_file == nullptr)
    {
      return Z_ERRNO;
    }

    m_filename = i_filename;

    if (i_build_index == true)
    {
      return BuildIndex();
    }

    return Z_OK;
  }

  int ZppRA::Open(FILE * i_file, bool i_build_index)
  {
    Close();

    m_cur_pos = 0;

    m_file = i_file;

    if (i_build_index == true)
    {
      return BuildIndex();
    }

    return Z_OK;
  }

  void ZppRA::Close()
  {
    if (m_index != nullptr)
    {
      free_index(m_index);
      m_index = nullptr;
    }

    if (m_file != nullptr && m_filename.empty() == false)
    {
      fclose(m_file);
    }

    m_file = nullptr;
    m_filename.clear();
    m_cur_pos = 0;
    m_buffer.clear();
    m_buffer_beg = 0;
  }

  ssize_t ZppRA::Read(std::vector<uint8_t> & o_data, const size_t i_count)
  {
    if (m_index == nullptr || m_file == nullptr)
    {
      return Z_ERRNO;
    }

    ssize_t ret = 0;

    ret = ReadOffset(o_data, i_count, m_cur_pos);
    if (ret < 0)
    {
      return ret;
    }

    m_cur_pos += ret;

    return ret;
  }

  ssize_t ZppRA::ReadOffset(std::vector<uint8_t> & o_data, const size_t i_count, const size_t i_offset)
  {
    if (m_index == nullptr || m_file == nullptr)
    {
      return Z_ERRNO;
    }

    ssize_t ret = 0;

    o_data.resize(i_count);

    ret = extract(m_file, m_index, i_offset, o_data.data(), o_data.size());

    if (ret < 0)
    {
      o_data.clear();
      return ret;
    }

    o_data.resize(ret);

    return ret;
  }

  int ZppRA::SetPos(const size_t i_pos)
  {
    if (m_index == nullptr)
    {
      return Z_ERRNO;
    }

    if (i_pos > m_index->uncompressed_size /*|| i_pos < 0*/)
    {
      return Z_ERRNO;
    }

    m_cur_pos = i_pos;

    return Z_OK;
  }

  size_t ZppRA::GetPos()
  {
    return m_cur_pos;
  }

  size_t ZppRA::GetSize()
  {
    if (m_index == nullptr)
    {
      return 0;
    }

    return m_index->uncompressed_size;
  }

  int ZppRA::SetBufferSize(const size_t i_size_backward, const size_t i_size_forward)
  {
    m_buffsize_backward = i_size_backward;
    m_buffsize_forward = i_size_forward;

    return 0;
  }

  bool ZppRA::GetFlagAllignBuffer()
  {
    return m_flag_align_buffer;
  }

  void ZppRA::SetFlagAllignBuffer(bool i_flag)
  {
    m_flag_align_buffer = i_flag;
  }

  int ZppRA::BuildIndex()
  {
    if (m_index != nullptr)
    {
      free_index(m_index);
      m_index = nullptr;
    }

    return build_index(m_file, SPAN, &m_index);
  }

  bool ZppRA::IsReady()
  {
    if (m_file == nullptr || m_index == nullptr)
    {
      return false;
    }

    return true;
  }

  uint8_t ZppRA::operator [](const size_t i_pos)
  {
    if (m_index == nullptr || m_file == nullptr)
    {
      return 0x00;
    }

    if (/*i_pos < 0 ||*/ i_pos >= m_index->uncompressed_size)
    {
      return 0x00;
    }

    if (m_flag_align_buffer == true)
    {
      if (PopulateBufferAlign(i_pos) != Z_OK)
      {
        return 0x00;
      }
    }
    else
    {
      if (PopulateBuffer(i_pos) != Z_OK)
      {
        return 0x00;
      }
    }

    if (i_pos - m_buffer_beg < m_buffer.size())
    {
      return m_buffer[i_pos - m_buffer_beg];
    }

    return 0x00;
  }

  void ZppRA::free_index(ZppRA::access * index)
  {
    if (index != NULL)
    {
      free(index->list);
      free(index);
    }
  }

  ZppRA::access *ZppRA::addpoint(ZppRA::access * index, int bits, off_t in, off_t out, unsigned left, unsigned char * window)
  {
    struct point *next;

    /* if list is empty, create it (start with eight points) */
    if (index == NULL)
    {
      index = (struct access*)malloc(sizeof(struct access));
      if (index == NULL) return NULL;
      index->list = (struct point*)malloc(sizeof(struct point) << 3);
      if (index->list == NULL)
      {
        free(index);
        return NULL;
      }
      index->size = 8;
      index->have = 0;
    }

    /* if list is full, make it bigger */
    else if (index->have == index->size)
    {
      index->size <<= 1;
      next = (struct point*)realloc(index->list, sizeof(struct point) * index->size);
      if (next == NULL)
      {
        free_index(index);
        return NULL;
      }
      index->list = next;
    }

    /* fill in entry and increment how many we have */
    next = index->list + index->have;
    next->bits = bits;
    next->in = in;
    next->out = out;
    if (left)
    {
      memcpy(next->window, window + WINSIZE - left, left);
    }
    if (left < WINSIZE)
    {
      memcpy(next->window + left, window, WINSIZE - left);
    }
    index->have++;

    /* return list, possibly reallocated */
    return index;
  }

  int ZppRA::build_index(FILE * in, off_t span, ZppRA::access ** built)
  {
    fseek(in, 0, SEEK_SET);
    int ret;
    off_t totin, totout;        /* our own total counters to avoid 4GB limit */
    off_t last;                 /* totout value of last access point */
    struct access *index;       /* access points being generated */
    z_stream strm;
    unsigned char input[CHUNK];
    unsigned char window[WINSIZE];

    /* initialize inflate */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, 47);      /* automatic zlib or gzip decoding */
    if (ret != Z_OK)
    {
      return ret;
    }

    /* inflate the input, maintain a sliding window, and build an index -- this
         also validates the integrity of the compressed data using the check
         information at the end of the gzip or zlib stream */
    totin = totout = last = 0;
    index = NULL;               /* will be allocated by first addpoint() */
    strm.avail_out = 0;
    do
    {
      /* get some compressed data from input file */
      strm.avail_in = fread(input, 1, CHUNK, in);
      if (ferror(in))
      {
        ret = Z_ERRNO;
        goto build_index_error;
      }
      if (strm.avail_in == 0)
      {
        ret = Z_DATA_ERROR;
        goto build_index_error;
      }
      strm.next_in = input;

      /* process all of that, or until end of stream */
      do
      {
        /* reset sliding window if necessary */
        if (strm.avail_out == 0)
        {
          strm.avail_out = WINSIZE;
          strm.next_out = window;
        }

        /* inflate until out of input, output, or at end of block --
                 update the total input and output counters */
        totin += strm.avail_in;
        totout += strm.avail_out;
        ret = inflate(&strm, Z_BLOCK);      /* return at end of block */
        totin -= strm.avail_in;
        totout -= strm.avail_out;
        if (ret == Z_NEED_DICT)
        {
          ret = Z_DATA_ERROR;
        }
        if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
        {
          goto build_index_error;
        }
        if (ret == Z_STREAM_END)
        {
          break;
        }

        /* if at end of block, consider adding an index entry (note that if
         data_type indicates an end-of-block, then all of the
         uncompressed data from that block has been delivered, and none
         of the compressed data after that block has been consumed,
         except for up to seven bits) -- the totout == 0 provides an
         entry point after the zlib or gzip header, and assures that the
         index always has at least one access point; we avoid creating an
         access point after the last block by checking bit 6 of data_type */
        if ((strm.data_type & 128)
            && !(strm.data_type & 64)
            && (totout == 0 || totout - last > span))
        {
          index = addpoint(index, strm.data_type & 7, totin,
                           totout, strm.avail_out, window);
          if (index == NULL)
          {
            ret = Z_MEM_ERROR;
            goto build_index_error;
          }
          last = totout;
        }
      } while (strm.avail_in != 0);
    } while (ret != Z_STREAM_END);

    /* clean up and return index (release unused entries in list) */
    index->compressed_size = strm.total_in;
    index->uncompressed_size = strm.total_out;
    (void)inflateEnd(&strm);
    index->list = (struct point*)realloc(index->list, sizeof(struct point) * index->have);
    index->size = index->have;
    *built = index;
    return index->size;

    /* return error */
build_index_error:
    (void)inflateEnd(&strm);
    if (index != NULL)
    {
      free_index(index);
    }
    return ret;
  }

  int ZppRA::extract(FILE * in, ZppRA::access * index, off_t offset, unsigned char * buf, int len)
  {
    int ret, skip;
    z_stream strm;
    struct point *here;
    unsigned char input[CHUNK];
    unsigned char discard[WINSIZE];

    /* proceed only if something reasonable to do */
    if (len < 0)
    {
      return 0;
    }

    /* find where in stream to start */
    here = index->list;
    ret = index->have;
    while (--ret && here[1].out <= offset)
    {
      here++;
    }

    /* initialize file and inflate state to start there */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, -15);         /* raw inflate */
    if (ret != Z_OK)
    {
      return ret;
    }
    ret = fseeko(in, here->in - (here->bits ? 1 : 0), SEEK_SET);
    if (ret == -1)
    {
      goto extract_ret;
    }
    if (here->bits)
    {
      ret = getc(in);
      if (ret == -1)
      {
        ret = ferror(in) ? Z_ERRNO : Z_DATA_ERROR;
        goto extract_ret;
      }
      (void)inflatePrime(&strm, here->bits, ret >> (8 - here->bits));
    }
    (void)inflateSetDictionary(&strm, here->window, WINSIZE);

    /* skip uncompressed bytes until offset reached, then satisfy request */
    offset -= here->out;
    strm.avail_in = 0;
    skip = 1;                               /* while skipping to offset */
    do
    {
      /* define where to put uncompressed data, and how much */
      if (offset == 0 && skip) /* at offset now */
      {
        strm.avail_out = len;
        strm.next_out = buf;
        skip = 0;                       /* only do this once */
      }
      if (offset > WINSIZE) /* skip WINSIZE bytes */
      {
        strm.avail_out = WINSIZE;
        strm.next_out = discard;
        offset -= WINSIZE;
      }
      else if (offset != 0) /* last skip */
      {
        strm.avail_out = (unsigned)offset;
        strm.next_out = discard;
        offset = 0;
      }

      /* uncompress until avail_out filled, or end of stream */
      do
      {
        if (strm.avail_in == 0)
        {
          strm.avail_in = fread(input, 1, CHUNK, in);
          if (ferror(in))
          {
            ret = Z_ERRNO;
            goto extract_ret;
          }
          if (strm.avail_in == 0)
          {
            ret = Z_DATA_ERROR;
            goto extract_ret;
          }
          strm.next_in = input;
        }
        ret = inflate(&strm, Z_NO_FLUSH);       /* normal inflate */
        if (ret == Z_NEED_DICT)
        {
          ret = Z_DATA_ERROR;
        }
        if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
        {
          goto extract_ret;
        }
        if (ret == Z_STREAM_END)
        {
          break;
        }
      } while (strm.avail_out != 0);

      /* if reach end of stream, then don't keep trying to get more */
      if (ret == Z_STREAM_END)
      {
        break;
      }

      /* do until offset reached and requested data read, or stream ends */
    } while (skip);

    /* compute number of uncompressed bytes read after offset */
    ret = skip ? 0 : len - strm.avail_out;

    /* clean up and return bytes read or error */
extract_ret:
    (void)inflateEnd(&strm);
    return ret;
  }

  int ZppRA::PopulateBuffer(const size_t i_pos)
  {
    if (m_buffer.empty() == true
        || m_buffer_beg > i_pos
        || (m_buffer_beg + m_buffer.size()) <= i_pos)
    {
      size_t new_buff_size = 1;
      if (m_buffsize_backward < i_pos)
      {
        m_buffer_beg = i_pos - m_buffsize_backward;
        new_buff_size += m_buffsize_backward;
      }
      else
      {
        m_buffer_beg = 0;
        new_buff_size += m_buffsize_backward - (i_pos + 1);
      }

      if (m_buffsize_forward < m_index->uncompressed_size - i_pos)
      {
        new_buff_size += m_buffsize_forward;
      }
      else
      {
        new_buff_size += m_buffsize_forward - (m_index->uncompressed_size - i_pos);
      }

      if (ReadOffset(m_buffer, new_buff_size, m_buffer_beg) < 0)
      {
        return Z_ERRNO;
      }
    }

    return Z_OK;
  }

  int ZppRA::PopulateBufferAlign(const size_t i_pos)
  {
    if (m_buffer.empty() == true
        || m_buffer_beg > i_pos
        || (m_buffer_beg + m_buffer.size()) <= i_pos)
    {
      size_t new_buff_size = 0;
      struct point * here = m_index->list;
      int ret = m_index->have;
      while (--ret && here[1].out <= static_cast<off_t>(i_pos))
      {
        here++;
      }

      if (ret == 0)
      {
        new_buff_size = m_index->uncompressed_size - static_cast<size_t>(here[0].out);
      }
      else
      {
        new_buff_size = static_cast<size_t>(here[1].out - here[0].out);
      }

      m_buffer_beg = static_cast<size_t>(here[0].out);

      if (ReadOffset(m_buffer, new_buff_size, m_buffer_beg) < 0)
      {
        return Z_ERRNO;
      }
    }

    return Z_OK;
  }

  ZppFW::ZppFW(const std::string & i_filename)
  {
    Open(i_filename);
  }

  ZppFW::ZppFW(FILE * i_file)
  {
    Open(i_file);
  }

  ZppFW::~ZppFW()
  {
    Close();
  }

  int ZppFW::Open(const std::string & i_filename)
  {
    Close();

    m_file = fopen(i_filename.c_str(), "wb");
    if (m_file == nullptr)
    {
      return Z_ERRNO;
    }

    m_filename = i_filename;

    int ret_val = InitZLib();
    if (ret_val != Z_OK)
    {
      return ret_val;
    }

    m_flag_error = false;
    return ret_val;
  }

  int ZppFW::Open(FILE * i_file)
  {
    Close();

    m_file = i_file;

    int ret_val = InitZLib();
    if (ret_val != Z_OK)
    {
      return ret_val;
    }

    m_flag_error = false;
    return ret_val;
  }

  void ZppFW::Close()
  {
    if (m_file != nullptr)
    {
      EndZLib();
    }

    if (m_file != nullptr && m_filename.empty() == false)
    {
      fclose(m_file);
    }

    m_file = nullptr;
    m_filename.clear();
    m_buffer.clear();
    m_stream = {};
  }

  int ZppFW::Write(const std::vector<uint8_t> & i_data)
  {
    return Write(i_data.data(), i_data.size());
  }

  int ZppFW::Write(const uint8_t * i_data, size_t i_size)
  {
    if (IsReady() == false)
    {
      return Z_ERRNO;
    }

    return compress(i_data, i_size);
  }

  size_t ZppFW::GetSize()
  {
    return m_stream.total_out;
  }

  bool ZppFW::GetFlagGzip()
  {
    return m_flag_gzip;
  }

  void ZppFW::SetFlagGzip(bool i_flag)
  {
    m_flag_gzip = i_flag;
  }

  int ZppFW::GetCompressionLevel()
  {
    return m_compression_level;
  }

  void ZppFW::SetCompressionLevel(int i_level)
  {
    m_compression_level = i_level;
  }

  size_t ZppFW::GetChunkSize()
  {
    return m_chunk_size;
  }

  void ZppFW::SetChunkSize(size_t i_size)
  {
    m_chunk_size = i_size;
  }

  const std::string &ZppFW::GetFilename()
  {
    return m_filename;
  }

  bool ZppFW::IsReady()
  {
    if (m_file == nullptr || ferror(m_file))
    {
      return false;
    }

    if (m_flag_error == true)
    {
      return false;
    }

    return true;
  }

  int ZppFW::InitZLib()
  {
    m_stream = {};
    m_stream.zalloc = Z_NULL;
    m_stream.zfree = Z_NULL;
    m_stream.opaque = Z_NULL;

    int ret_val = Z_ERRNO;
    if (m_flag_gzip == true)
    {
      ret_val = deflateInit2(&m_stream, m_compression_level, Z_DEFLATED, windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY);
      if(ret_val != Z_OK)
      {
        return ret_val;
      }
    }
    else
    {
      ret_val = deflateInit(&m_stream, m_compression_level);
      if(ret_val != Z_OK)
      {
        return ret_val;
      }
    }

    m_buffer = std::vector<uint8_t>(m_chunk_size);

    m_stream.next_out = m_buffer.data();
    m_stream.avail_out = static_cast<unsigned int>(m_buffer.size());

    return ret_val;
  }

  int ZppFW::EndZLib()
  {
    int flush = Z_FINISH;
    std::vector<uint8_t> temp_data;

    m_stream.avail_in = static_cast<unsigned int>(temp_data.size());
    m_stream.next_in = temp_data.data();

    int deflate_res = Z_OK;
    while (deflate_res == Z_OK)
    {
      if (m_stream.avail_out == 0)
      {
        if(fwrite(m_buffer.data(), 1, m_buffer.size(), m_file) != m_buffer.size()
           || ferror(m_file))
        {
          deflateEnd(&m_stream);
          m_stream = {};
          m_flag_error = true;
          return Z_ERRNO;
        }

        m_stream.next_out = m_buffer.data();
        m_stream.avail_out = static_cast<unsigned int>(m_buffer.size());
      }
      deflate_res = deflate(&m_stream, flush);
      if (deflate_res == Z_STREAM_ERROR)
      {
        deflateEnd(&m_stream);
        m_stream = {};
        m_flag_error = true;
        return deflate_res;
      }
    }

    size_t nbytes = m_buffer.size() - m_stream.avail_out;
    if(fwrite(m_buffer.data(), 1, nbytes, m_file) != nbytes
       || ferror(m_file))
    {
      deflateEnd(&m_stream);
      m_stream = {};
      m_flag_error = true;
      return Z_ERRNO;
    }
    deflateEnd(&m_stream);
    m_stream = {};

    return Z_OK;
  }

  int ZppFW::compress(const uint8_t * i_data, size_t i_size)
  {
    int flush = Z_NO_FLUSH;

    m_stream.avail_in = static_cast<unsigned int>(i_size);
    m_stream.next_in = const_cast<unsigned char *>(i_data);

    while (m_stream.avail_in != 0)
    {
      int deflate_res = deflate(&m_stream, flush);
      if (deflate_res == Z_STREAM_ERROR)
      {
        deflateEnd(&m_stream);
        m_stream = {};
        m_flag_error = true;
        return deflate_res;
      }

      if (m_stream.avail_out == 0)
      {
        if(fwrite(m_buffer.data(), 1, m_buffer.size(), m_file) != m_buffer.size()
           || ferror(m_file))
        {
          deflateEnd(&m_stream);
          m_stream = {};
          m_flag_error = true;
          return Z_ERRNO;
        }
        m_stream.next_out = m_buffer.data();
        m_stream.avail_out = static_cast<unsigned int>(m_buffer.size());
      }
    }

    return true;
  }
}

