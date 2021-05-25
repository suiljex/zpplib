#ifndef ZPPLIB_HPP
#define ZPPLIB_HPP

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <vector>
#include <list>
#include <string.h>

#include <zlib.h>

#define CHUNK_SIZE 4096
#define COMPRESSION_LEVEL 9

namespace slx {

  class ZppRA
  {
  public:
    ZppRA() = default;

    ZppRA
    (
        const std::string & i_filename
    );

    ZppRA
    (
        FILE * i_file
    );

    ~ZppRA();

    int Open
    (
        const std::string & i_filename
        , bool i_build_index = true
    );

    int Open
    (
        FILE * i_file
        , bool i_build_index = true
    );

    void Close();

    ssize_t Read
    (
        std::vector<uint8_t> & o_data
        , const size_t i_count
    );

    ssize_t ReadOffset
    (
        std::vector<uint8_t> & o_data
        , const size_t i_count
        , const size_t i_offset
    );

    int SetPos
    (
        const size_t i_pos
    );

    size_t GetPos();

    size_t GetSize();

    int SetBufferSize
    (
        const size_t i_size_backward
        , const size_t i_size_forward
    );

    int BuildIndex();

    bool IsReady();

    uint8_t operator [] (const size_t i_pos);

  protected:
    static const ssize_t SPAN    = 1048576L;      /* desired distance between access points */
    static const ssize_t WINSIZE = 32768U;        /* sliding window size */
    static const ssize_t CHUNK   = 16384;         /* file input buffer size */

    /* access point entry */
    struct point
    {
      off_t out;          /* corresponding offset in uncompressed data */
      off_t in;           /* offset in input file of first full byte */
      int bits;           /* number of bits (1-7) from byte at in - 1, or 0 */
      unsigned char window[WINSIZE];  /* preceding 32K of uncompressed data */
    };

    /* access point list */
    struct access
    {
      int have;           /* number of list entries filled in */
      int size;           /* number of list entries allocated */
      struct point *list; /* allocated list */
      size_t compressed_size;
      size_t uncompressed_size;
    };

    /* Deallocate an index built by build_index() */
    static void free_index(struct access *index);

    /* Add an entry to the access point list.  If out of memory, deallocate the
     existing list and return NULL. */
    static struct access *addpoint(struct access *index, int bits,
                                   off_t in, off_t out, unsigned left, unsigned char *window);

    /* Make one entire pass through the compressed stream and build an index, with
     access points about every span bytes of uncompressed output -- span is
     chosen to balance the speed of random access against the memory requirements
     of the list, about 32K bytes per access point.  Note that data after the end
     of the first zlib or gzip stream in the file is ignored.  build_index()
     returns the number of access points on success (>= 1), Z_MEM_ERROR for out
     of memory, Z_DATA_ERROR for an error in the input file, or Z_ERRNO for a
     file read error.  On success, *built points to the resulting index. */
    static int build_index(FILE *in, off_t span, struct access **built);

    /* Use the index to read len bytes from offset into buf, return bytes read or
     negative for error (Z_DATA_ERROR or Z_MEM_ERROR).  If data is requested past
     the end of the uncompressed data, then extract() will return a value less
     than len, indicating how much as actually read into buf.  This function
     should not return a data error unless the file was modified since the index
     was generated.  extract() may also return Z_ERRNO if there is an error on
     reading or seeking the input file. */
    static int extract(FILE *in, struct access *index, off_t offset,
                       unsigned char *buf, int len);

    std::string m_filename;
    FILE * m_file = nullptr;
    size_t m_cur_pos = 0;
    struct access * m_index = nullptr;

    size_t m_buffsize_backward = 0; //1048576L
    size_t m_buffsize_forward = 0;  //1048576L
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_beg = 0;
  };

  class ZppFW
  {
  public:
    ZppFW() = default;

    ZppFW
    (
        const std::string & i_filename
    );

    ZppFW
    (
        FILE * i_file
    );

    ~ZppFW();

    int Open
    (
        const std::string & i_filename
    );

    int Open
    (
        FILE * i_file
    );

    void Close();

    ssize_t Write
    (
        const std::vector<uint8_t> & i_data
    );

    ssize_t Write
    (
        const uint8_t * i_data
      , size_t i_size
    );

    size_t GetSize();

    bool IsReady();

  protected:
    int InitZLib();

    int EndZLib();

    bool compress(const uint8_t * i_data, size_t i_size);

    std::vector<uint8_t> m_buffer;// = std::vector<uint8_t>(CHUNK_SIZE);
    FILE * m_file = nullptr;
    std::string m_filename;

    z_stream m_stream = {};
  };
}

#endif // ZPPLIB_HPP
