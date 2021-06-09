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

namespace slx
{
  //! Класс чтения файлов, сжатых zlib
  class ZppReader
  {
  public:
    //! Конструктор
    ZppReader() = default;

    //! Конструктор
    /*!
       Открывает файл на чтение
     */
    ZppReader
    (
        const std::string & i_filename //!< [in] Имя файла
    );

    //! Конструктор
    /*!
       Открывает файл на чтение
     */
    ZppReader
    (
        FILE * i_file //!< [in] Дескриптор файла
    );

    ~ZppReader();

    //! Открыть файл
    /*!
       Открывает файл на чтение
     */
    int Open
    (
        const std::string & i_filename //!< [in] Имя файла
      , bool i_build_index = true //!< Флаг, создавать ли индекс при открытии
    );

    //! Открыть файл
    /*!
       Открывает файл на чтение
     */
    int Open
    (
        FILE * i_file //!< [in] Дескриптор файла
      , bool i_build_index = true //!< Флаг, создавать ли индекс при открытии
    );

    //! Закрыть файл
    void Close();

    //! Прочитать данные
    /*!
       Последовательное чтение, обновляется текущая позиция.
       Считывается o_data.size() байт

       \return Количество считанных байт
     */
    ssize_t Read
    (
        std::vector<uint8_t> & o_data //!< [out] Вектор, в который будут записаны данные
    );

    //! Прочитать данные
    /*!
       Последовательное чтение, обновляется текущая позиция.
       Считывается i_count байт

       \return Количество считанных байт
     */
    ssize_t Read
    (
        std::vector<uint8_t> & o_data //!< [out] Вектор, в который будут записаны данные
      , const size_t i_count //!< [in] Количество байт для считывания
    );

    //! Прочитать данные
    /*!
       Последовательное чтение, обновляется текущая позиция.
       Считывается i_count байт

       \return Количество считанных байт
     */
    ssize_t Read
    (
        uint8_t * o_data //!< [out] Массив, в который будут записаны данные
      , const size_t i_count //!< [in] Количество байт для считывания
    );

    //! Прочитать данные
    /*!
       Чтение данных по смещению
       Считывается o_data.size() байт

       \return Количество считанных байт
     */
    ssize_t ReadOffset
    (
        std::vector<uint8_t> & o_data //!< [out] Вектор, в который будут записаны данные
      , const size_t i_offset //!< [in] Смещение
    );

    //! Прочитать данные
    /*!
       Чтение данных по смещению
       Считывается i_count байт

       \return Количество считанных байт
     */
    ssize_t ReadOffset
    (
        std::vector<uint8_t> & o_data //!< [out] Вектор, в который будут записаны данные
      , const size_t i_count //!< [in] Количество байт для считывания
      , const size_t i_offset //!< [in] Смещение
    );

    //! Прочитать данные
    /*!
       Чтение данных по смещению
       Считывается i_count байт

       \return Количество считанных байт
     */
    ssize_t ReadOffset
    (
        uint8_t * o_data  //!< [out] Массив, в который будут записаны данные
      , const size_t i_count //!< [in] Количество байт для считывания
      , const size_t i_offset //!< [in] Смещение
    );

    //! Установить текущую позицию
    /*!

     */
    int SetPos
    (
        const size_t i_pos //!< [in] Позиция
    );

    //! Получить текущую позицию
    /*!
      \return Позиция
     */
    size_t GetPos();

    //! Получить размер файла
    /*!
      \return Размер файла
     */
    size_t GetSize();

    //! Установить размеры буфера
    /*!
     */
    int SetBufferSize
    (
        const size_t i_size_backward //!< [in] Количество байт для кэширования до запрашиваемой позиции
      , const size_t i_size_forward //!< [in] Количество байт для кэширования после запрашиваемой позиции
    );

    //! Получить размер буфера
    /*!
      \return Размер буфера
     */
    size_t GetBufferSize();

    //! Получить значение флага выравнивания по границам считываемых данных
    /*!
      \return значение флага выравнивания по границам считываемых данных
     */
    bool GetFlagAllignBuffer();

    //! Установить значение флага выравнивания по границам считываемых данных
    /*!
     */
    void SetFlagAllignBuffer
    (
        bool i_flag //!< [in] Флаг выравнивания по границам считываемых данных
    );

    //! Построить индекс
    /*!
     */
    int BuildIndex();

    //! Получить имя файла
    /*!
      \return Имя файла
     */
    const std::string & GetFilename();

    //! Получить статус готовности
    /*!
      \return Статус готовности
     */
    bool IsReady();

    //! Вернуть байт по индексу
    /*!
      Целесообразно перед чтением проверять IsReady() и GetSize()

      \return значния байта
      \return 0x00 в случае ошибки
     */
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

    int PopulateBuffer
    (
        const size_t i_pos
    );

    int PopulateBufferAlign
    (
        const size_t i_pos
    );

    std::string m_filename;
    FILE * m_file = nullptr;
    size_t m_cur_pos = 0;
    struct access * m_index = nullptr;

    size_t m_buffsize_backward = 0; //1048576L
    size_t m_buffsize_forward = 0;  //1048576L
    bool m_flag_align_buffer = true;
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_beg = 0;
  };

  //! Класс записи файлов, со сжатием zlib
  /*!
     Применение изменения конфигурации происходит только при открытии файла
   */
  class ZppWriter
  {
  public:
    //! Конструктор
    ZppWriter() = default;

    //! Конструктор
    /*!
       Открывает файл на запись
     */
    ZppWriter
    (
        const std::string & i_filename //!< [in] Имя файла
    );

    //! Конструктор
    /*!
       Открывает файл на запись
     */
    ZppWriter
    (
        FILE * i_file //!< [in] Дескриптор файла
    );

    //! Деструктор
    ~ZppWriter();

    //! Открыть файл
    /*!
       Открывает файл на запись
     */
    int Open
    (
        const std::string & i_filename //!< [in] Имя файла
    );

    //! Открыть файл
    /*!
       Открывает файл на запись
     */
    int Open
    (
        FILE * i_file //!< [in] Дескриптор файла
    );

    //! Закрыть файл
    void Close();

    //! Записать данные
    /*!
       Записывается i_data.size() байт

       \return Количество записанных байт
     */
    int Write
    (
        const std::vector<uint8_t> & i_data //!< [in] Вектор с данными для записи
    );

    //! Записать данные
    /*!
       Записывается i_size байт

       \return Количество записанных байт
     */
    int Write
    (
        const uint8_t * i_data //!< [in] Массив с данными для записи
      , size_t i_size //!< [in] Количество байт для записи
    );

    //! Получить размер файла
    /*!
      \return Размер файла
     */
    size_t GetSize();

    //! Получить значения флага совместимости с GZip
    /*!
      \return Значение флага
     */
    bool GetFlagGzip();

    //! Установить значения флага совместимости с GZip
    /*!
     */
    void SetFlagGzip
    (
        bool i_flag //!< [in] Значения флага совместимости с GZip
    );

    //! Получить уровень сжатия
    /*!
      \return Уровень сжатия
     */
    int GetCompressionLevel();

    //! Установить уровень сжатия
    /*!
     */
    void SetCompressionLevel
    (
        int i_level //!< [in] Уровень сжатия
    );

    //! Получить размер блока данных
    /*!
      \return Размер блока данных
     */
    size_t GetChunkSize();

    //! Установить размер блока данных
    /*!
     */
    void SetChunkSize
    (
        size_t i_size //!< [in] Размер блока данных
    );

    //! Получить имя файла
    /*!
      \return Имя файла
     */
    const std::string & GetFilename();

    //! Получить статус готовности
    /*!
      \return Статус готовности
     */
    bool IsReady();

  protected:
    int InitZLib();

    int EndZLib();

    int compress(const uint8_t * i_data, size_t i_size);

    std::vector<uint8_t> m_buffer;
    FILE * m_file = nullptr;
    std::string m_filename;
    int m_compression_level = Z_BEST_COMPRESSION;
    size_t m_chunk_size = 4096;
    bool m_flag_gzip = true;

    bool m_flag_error = true;

    z_stream m_stream = {};
  };
}

#endif // ZPPLIB_HPP
