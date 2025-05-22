#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include <sstream>
#include <string>
#include <vector>
#include <future>
#include <algorithm>
#include <iomanip>
#include <regex>
//#include <stdexcept>
#include <fstream>


#include "MaMpdMute.hxx"
#include "util/ConRaat.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
static constexpr Domain con_domain("MaMpdMute");
// 文件路径
const std::string FILE_PATH = "/tmp/ma_audio";



static std::string readFromFile();
static bool writeToFile(const std::string& content); 
//std::atomic<bool> isPlaying(false);
//static std::string audio;  //记录当前的采样率
static std::string audio = readFromFile();  //mpd 异常重启 重新去读一下上一次的配置
static std::string maDacDelayStatus = "off";
static std::string maOutput = "digital"; // 只有是DAC的时候才生效
static std::string maUsbLimit = "768k"; // DAC限制后的采样率
static std::mutex ma_mpdmute_mutex;



// 写入文件内容（会覆盖原内容）
static bool writeToFile(const std::string& content) {
    std::ofstream outFile(FILE_PATH, std::ios::trunc); // 以截断模式打开，覆盖原内容
    if (!outFile) {
        return false; // 文件打开失败，返回 false
    }
    outFile << content;
    outFile.close();
    return true; // 写入成功，返回 true
}

// 从文件读取内容
static std::string readFromFile() {
    std::ifstream inFile(FILE_PATH, std::ios::binary); // 以二进制模式打开文件，防止编码问题
    if (!inFile) {
        return ""; // 如果文件打开失败，返回空字符串
    }
    // 使用 std::istreambuf_iterator 读取整个文件的内容
    std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();
    return content; // 返回文件的内容
}
void handle_mpd_mute_setDacDelay(std::string dacdelay)
{
    {
        std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
        maDacDelayStatus = dacdelay; 
    }
    FormatNotice(con_domain, "maDacDelayStatus %s",dacdelay.c_str()); 
}
//设置输出
void handle_mpd_mute_setOutput(std::string output)
{
    {
        std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
        maOutput = output; 
    }
    FormatNotice(con_domain, "maOutput %s",output.c_str()); 
}
bool handle_mpd_mute_getDacDelay()
{
    std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
    if ((maOutput.compare("DAC") ==0) && (maDacDelayStatus.compare("on") == 0))
    {
        return true;    
    }    
    return false;  
}

// 获取 maDacDelayStatus 的值
std::string handle_mpd_mute_getDacDelay_str()
{
    std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
    return maDacDelayStatus;
}
// 获取 maOutput 的值
std::string handle_mpd_mute_getOutput_str()
{
    std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
    return maOutput;
}

void handle_mpd_mute_setUsbLimit(std::string usblimit)
{
    {
        std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
        maUsbLimit = usblimit; 
    }
    FormatNotice(con_domain, "maUsbLimit %s",maUsbLimit.c_str()); 
}
// 获取 maUsbLimit 的值
std::string handle_mpd_mute_getUsbLimit_str()
{
    std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
    return maUsbLimit;
}

static bool isStringInLines(const std::string& str) {
    std::vector<std::string> lines = {
        "dsd64",
		"dsd128",
		"dsd256",
		"dsd512",
        "44100",
        "48000",
		"96000",
        "768000",
		"176400",
		"192000",		
        "352800",
		"384000",
        "88200",
		"705600"
    };
    // 使用 std::find 查找 str 是否在 lines 中
    return std::find(lines.begin(), lines.end(), str) != lines.end();
}


static bool isPureNumber(const std::string& str) {
    std::regex number_regex("^[0-9]+$");
    return std::regex_match(str, number_regex);
}

// 1相等 2 大于 3小于
static int isGreaterThanMaUsbLimit(std::string input, std::string maUsbLimit_in, std::string& maUsbLimit_out) {
    if (input.empty() || input.size() <= 1 || !isPureNumber(input)) {
        return -1; 
    }

    try {
        double value;
        // 检查 maUsbLimit_in 是否以 'k' 结尾
        if (maUsbLimit_in.back() == 'k') {
            value = std::stod(maUsbLimit_in.substr(0, maUsbLimit_in.size() - 1)) * 1000;
        } else {
            value = std::stod(maUsbLimit_in);
        }

        // 将转换后的限制值转换为字符串
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << value; // 设置为固定小数点格式，不保留小数位
        maUsbLimit_out = oss.str();

        // 将输入转换为浮点数
        double inputValue = std::stod(input);

        // 比较大小并返回结果
        if (inputValue == value) {
            return 1; // 相等
        } else if (inputValue > value) {
            return 2; // 大于
        } else {
            return 3; // 小于
        }
    }
    catch (const std::invalid_argument& e) {
        return -1; //std::stod 转换失败
    }
    catch (const std::out_of_range& e) {
        return -1; //std::stod 超出范围
    }
}

bool  handle_mpd_mute_parseLine_fist(std::string line) 
{

    std::istringstream ss(line);
    std::string firstValue;
    // 分割字符串	
    std::getline(ss, firstValue, ':');
	// 检查 testString 是否在 lines 中
    //if (isStringInLines(firstValue)) 
    {
        if (firstValue.find("dsd") == 0)
        {
            if (audio.compare(firstValue) != 0)
            {   
                std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
                audio = firstValue;
                writeToFile(audio);
                return true;
            }
            else
            {
                return false;
            }
        }
        // else if (firstValue.find("*") == 0)
        // {
        //     // if (audio.compare(firstValue) != 0)
        //     // {   
        //     //     std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
        //     //     audio = firstValue;
        //     //     writeToFile(audio);
        //     //     return true;
        //     // }
        //     // else
        //     {
        //         fprintf(stderr, "LELE: audio3 out= **");
        //         return false;
        //     }
        // }
        else
        {

            std::string maUsbLimit_sub;
            std::string maUsbLimitformtar;

            {
                std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
                maUsbLimit_sub = maUsbLimit;
            }
            int iret = isGreaterThanMaUsbLimit(firstValue,maUsbLimit_sub,maUsbLimitformtar);
            FormatDebug(con_domain, "firstValue11: %s maUsbLimit: %s maUsbLimitformtar: %s audio: %s",
                        firstValue.c_str(), maUsbLimit_sub.c_str(), maUsbLimitformtar.c_str(), audio.c_str());

            if (iret == 1 || iret == 2) //传入的 采样率大于或者等于 DAC限制采样率 进行判断
            {
                if (audio.compare(maUsbLimitformtar) != 0)
                {
                    {
                        std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
                        audio = maUsbLimitformtar;
                        writeToFile(audio);
                    }
                    
                    FormatDebug(con_domain, "firstValue12: %s maUsbLimit: %s maUsbLimitformtar: %s audio: %s",
                        firstValue.c_str(), maUsbLimit_sub.c_str(), maUsbLimitformtar.c_str(), audio.c_str());
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else if (iret == 3) //传入的 采样率小于 DAC限制采样率
            {
                if ( audio.compare(firstValue) != 0)
                {
                    {
                        std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
                        audio = firstValue;
                        writeToFile(audio);
                    }     
                    FormatDebug(con_domain, "firstValue13: %s maUsbLimit: %s maUsbLimitformtar: %s audio: %s",
                        firstValue.c_str(), maUsbLimit_sub.c_str(), maUsbLimitformtar.c_str(), audio.c_str());
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                FormatDebug(con_domain, "firstValue error: %s maUsbLimit: %s maUsbLimitformtar: %s audio: %s",
                        firstValue.c_str(), maUsbLimit_sub.c_str(), maUsbLimitformtar.c_str(), audio.c_str());
                return false;
            }   
        }
        
        
        {
            std::lock_guard<std::mutex> lock(ma_mpdmute_mutex);
            audio = firstValue;
            writeToFile(audio);
        }  
        return false;
    }
	return false;
}
// bool  handle_mpd_mute_parseLine_fist_2(std::string line) 
// {

//     std::istringstream ss(line);
//     std::string firstValue;
//     // 分割字符串	
//     std::getline(ss, firstValue, ':');
// 	// 检查 testString 是否在 lines 中
//     if (isStringInLines(firstValue) &&  audio.compare(firstValue) != 0) {
// 		audio = firstValue;
//         return true;
//     }
// 	return false;
// }
int handle_mpd_mute_parseLine_do(void) 
{      
    handle_send_maPlayerAction(audio,audio);
    return 0;
}
//newAudio  都送一样,暂时保留oldAudio ma_server用不到
//oldAudio
void handle_send_maPlayerAction(std::string audio_old,std::string audio_new)
{
    std::string retbuf = "{\"cmd\":\"";
    retbuf += "maAudioAction";
    retbuf +="\",\"value\":\"";
    retbuf += "audio";
    retbuf +="\",\"oldAudio\":\"";
    retbuf += audio_old;
    retbuf +="\",\"newAudio\":\"";
    retbuf += audio_new;
    retbuf += "\"";
    retbuf += "}\r\nOK\r\n";
	std::string mstSocket("/opt/mads-sock");
	ConRaat cr(-1, retbuf, mstSocket);
	cr.send_ctl();
	std::string rts;
	rts = cr.get_mads_status(rts);
}

