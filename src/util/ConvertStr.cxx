#include "ConvertStr.hxx"

int convert_big5ToGbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len)
{
	iconv_t cd;
	char **pin = &src_str;
	char **pout = &dst_str;

	//cd = iconv_open("utf8", "gbk");
    cd = iconv_open("gbk", "big5");
	if (cd == 0)
		return -1;
	memset(dst_str, '\0', dst_len);
	if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
	{
		iconv_close(cd);
		return -1;
	}
	iconv_close(cd);

	**pout = '\0';

	return 0;
}

int convert_GBkToUtf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len)
{
	iconv_t cd;
	char **pin = &src_str;
	char **pout = &dst_str;

	cd = iconv_open("utf8", "gbk");
	if (cd == 0)
		return -1;
	memset(dst_str, '\0', dst_len);
	if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
	{
		iconv_close(cd);
		return -1;
	}
	iconv_close(cd);

	**pout = '\0';

	return 0;
}

int convert_GBkToGBK(char *src_str, size_t src_len, char *dst_str, size_t dst_len)
{
	iconv_t cd;
	char **pin = &src_str;
	char **pout = &dst_str;

	cd = iconv_open("gbk", "gbk");
	if (cd == 0)
		return -1;
	memset(dst_str, '\0', dst_len);
	if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
	{
		iconv_close(cd);
		return -1;
	}
	iconv_close(cd);

	**pout = '\0';

	return 0;
}

int convert_utf8Toutf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len)
{
	iconv_t cd;
	char **pin = &src_str;
	char **pout = &dst_str;

	cd = iconv_open("utf8", "utf8");
	if (cd == 0)
		return -1;
	memset(dst_str, '\0', dst_len);
	if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
	{
		iconv_close(cd);
		return -1;
	}
	iconv_close(cd);

	**pout = '\0';

	return 0;
}

int convert_isUTF8(char *str, size_t len)
{
	char *utf8 = new char[len + 8];
	if(utf8 == nullptr) return -1;
	int ret = 0;

	if( 0 == convert_utf8Toutf8(str, len, utf8, len + 8))
	{
		ret = 1;
	}
	else
	{
		ret = 0;
	}
	delete [] utf8;
	return ret;
}

int convert_convertstr(char *buf, size_t src_len, char *utf, size_t dst_len)
{
	//char gbk[4096];
	char *gbk = new char[dst_len];
	if(gbk == nullptr) return -1;
	int ret = 0;
	
	if(0 == convert_GBkToGBK(buf, src_len, gbk, dst_len))
	{
		if(0 == convert_GBkToUtf8(gbk, strlen(gbk), utf, dst_len))
		{
			printf("gbk to utf8 ok::: %s\n", utf);
		}
		else ret = -1;
	}
	else
	{
		printf("gbk to gbk failed\n");
		if(0 == convert_big5ToGbk(buf, src_len, gbk, dst_len))
		{
			if(0 == convert_GBkToUtf8(gbk, strlen(gbk), utf, dst_len))
			{
				printf("gbk to utf8 ok::: %s\n", utf);
			}
			else ret = -1;
		}
		else
		{
			printf("big5 to gbk failed\n");
			ret = -1;
		}
	}
	delete [] gbk;
	return ret;
}

static int loc_mpd_softwareVolume = 30;
static bool loc_mpd_softwareVolumeStatus = false;

void mpd_softwareVolume_set(int vol)
{
	loc_mpd_softwareVolume = vol;
}

int mpd_softwareVolume_get(void)
{
	return loc_mpd_softwareVolume;
}

void mpd_softwareVolumeStatus_set(bool status)
{
	loc_mpd_softwareVolumeStatus = status;
}

bool mpd_softwareVolumeStatus_get(void)
{
	return loc_mpd_softwareVolumeStatus;
}
