
#ifdef WINDOWS
#ifdef QMDECRYPTION_EXPORTS
#define QMDECRYPTION_API __declspec(dllexport)
#else
#define QMDECRYPTION_API __declspec(dllimport)
#endif
#else
#ifdef QMDECRYPTION_EXPORTS
#define QMDECRYPTION_API __attribute__((visibility("default")))
#else
#define QMDECRYPTION_API
#endif
#endif

typedef void* DecryptionHandle;
/*
   创建解密器
   keyBuf: ekey字符串
   keySize: ekey长度
*/
extern "C" QMDECRYPTION_API DecryptionHandle CreateDecryption(const unsigned char* keyBuf, int keySize);

/*
	解密， 
	index_offset: 数据在整个文件中的位置
	in_buffer: 缓存， 
	in_size: 数据大小
	return: 解密后数据大小，解密前后数据大小肯定是相等的。
*/
extern "C" QMDECRYPTION_API int Decrypt(DecryptionHandle handle, unsigned long long index_offset, char* in_buffer, int in_size);
/*
   释放解密器
*/
extern "C" QMDECRYPTION_API void DestroyDecryption(DecryptionHandle handle);
