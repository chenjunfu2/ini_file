#pragma once

#include <unordered_map>
#include <stdio.h>
#include <ctype.h>
#include <string>


class ini_file
{
public:
	typedef std::string KeyName, KeyValue, SecName, FileName, ReadData;

	typedef std::unordered_map<KeyName, KeyValue> KeyList;
	typedef std::unordered_map<SecName, KeyList> SecList;
private:
	SecList csSecList;
protected:
	enum class ReadStatus//:int
	{
		new_line,//新行
		section_name,//节名
		section_next,//节后
		key_name,//键名
		key_value,//键值
		raw_key_value,//原始值（可以包含空格、换行符的原始值，使用"包围）
		raw_key_next,//原始值后
		line_end,//行尾
		comment,//注释
		error,//错误
	};

	//跳过直到谓词返回true
	template<typename T>
	static bool JumpIf(FILE *f, T t, bool unget = true)//t为一元谓词
	{
		int c;//这里不使用char的原因是为了判断EOF，这样即便文件内存在0xff也不影响EOF的判断
		while ((c = fgetc(f)) != EOF)
		{
			if (t(c))
			{
				if (unget == true)
				{
					ungetc(c, f);//回退
				}
				return true;
			}
		}

		return false;//遇到文件尾
	}

	static bool IfNextNoSpace(int c)//下一个非空字符，可越过行
	{
		return !isspace(c);
	}

	static bool IfNextLine(int c)//下一行
	{
		return c == '\n';
	}

	static bool IfNextComment(int c)//下一行
	{
		return c == '\"';
	}

	//读取直到谓词返回false
	template<typename T>
	static bool ReadUntil(ReadData &rd, FILE *f, T t, bool unget = true)
	{
		int c;//这里不使用char的原因是为了判断EOF，这样即便文件内存在0xff也不影响EOF的判断
		while ((c = fgetc(f)) != EOF)
		{
			if (!t(c))
			{
				if (unget == true)
				{
					ungetc(c, f);//回退
				}
				else
				{
					rd.push_back(c);//放入
				}
				return true;
			}

			rd.push_back(c);
		}

		return false;//遇到文件尾
	}

	static bool UntilSectionNext(int c)
	{
		return c != ']' && c != '\n';//没到新行并且没遇到sec结尾
	}

	static bool UntilRawKeyNext(int c)
	{
		return c != '\"';
	}

	//返回true则continue一次
	static bool StatusTransition(ReadStatus& enStatus, int c)
	{
		switch (c)
		{
		case '['://节名
			{
				if (enStatus == ReadStatus::new_line)
				{
					enStatus = ReadStatus::section_name;
					break;
				}
				enStatus = ReadStatus::error;//出错
			}
			break;
		case ']'://节后
			{
				if (enStatus == ReadStatus::section_name)
				{
					enStatus = ReadStatus::section_next;
					return true;
				}
				enStatus = ReadStatus::error;//出错
			}
			break;
		case '#'://注释
		case ';':
			{
				enStatus = ReadStatus::comment;
			}
			break;
		case '='://键值
			{
				if (enStatus == ReadStatus::key_name)
				{
					enStatus = ReadStatus::key_value;
					return true;
				}
				enStatus = ReadStatus::error;//出错
			}
			break;
		case '\"'://原始值
			{
				if (enStatus == ReadStatus::key_value)
				{
					enStatus = ReadStatus::raw_key_value;
					break;
				}
				else if (enStatus == ReadStatus::raw_key_value)
				{
					enStatus = ReadStatus::raw_key_next;
					return true;//舍弃末尾"字符
				}
				enStatus = ReadStatus::error;
			}
			break;
		case '\n'://行尾
			{
				enStatus = ReadStatus::line_end;
			}
			break;
		default:
			{
				if (enStatus == ReadStatus::new_line)//新行
				{
					enStatus = ReadStatus::key_name;//键名
					break;
				}
			}
			break;
		}

		return false;
	}

public:
	ini_file(void) :csSecList({{"",KeyList{}}})//默认存在一个空节
	{}
	~ini_file(void) = default;
	ini_file(const ini_file &_Other) :csSecList(_Other.csSecList)
	{}
	ini_file(ini_file &&_Other) noexcept :csSecList(std::move(_Other.csSecList))
	{}
	ini_file operator=(const ini_file &_Other)
	{
		csSecList = _Other.csSecList;
	}
	ini_file operator=(ini_file &&_Other) noexcept
	{
		csSecList = std::move(_Other.csSecList);
	}

	/// <summary>
	/// 从ini文件中读取配置，
	/// 如果配置文件内存在非法内容，则忽略直到合法内容
	/// 文件名使用"CON"(Windows)、"/dev/console"(Linux)则从标准输入读入
	/// </summary>
	/// <param name="strFileName">文件名</param>
	/// <returns>读取是否成功</returns>
	bool Read(const FileName &strFileName)
	{
		FILE *fRead = fopen(strFileName.c_str(), "r");//打开只读文件
		if (fRead == NULL)
		{
			return false;
		}

		ReadStatus enStatus = ReadStatus::new_line, enLastStatus;
		SecName strSecName = "", strLastSecName = strSecName;
		KeyName strKeyName;
		KeyValue strKeyValue;
		KeyList csKeyList;
		int c;
		
		if (!JumpIf(fRead, IfNextNoSpace, true))//忽略直到下一个非空字符
		{
			return true;
		}

		while (true)
		{
			enLastStatus = enStatus;
			if ((c = fgetc(fRead)) == EOF)
			{
				enStatus = ReadStatus::line_end;//EOF最后当行尾处理一次，并在处理例程内设置bContinue为false
			}
			else if (StatusTransition(enStatus, c))//转换状态
			{
				continue;
			}

			switch (enStatus)
			{
			default://严重错误（运行时数据错误）
			case ReadStatus::new_line://新行
				{
					return fclose(fRead), false;//函数返回
				}
				break;//进入错误处理
			case ReadStatus::section_name://节名
				{
					if (!ReadUntil(strSecName, fRead, UntilSectionNext, true))
					{
						break;//错误处理
					}
				}
				continue;//处理下一个字符
			case ReadStatus::section_next://节后
			case ReadStatus::raw_key_next://原始键值后
				{
					if (isspace(c))
					{
						continue;
					}
				}
				break;//进入错误处理
			case ReadStatus::key_name://键名
				{
					strKeyName.push_back(c);
				}
				continue;//处理下一个字符
			case ReadStatus::key_value://键值
				{
					if (isspace(c))
					{
						break;//进入错误处理
					}
					//压入尾部
					strKeyValue.push_back(c);
				}
				continue;//处理下一个字符
			case ReadStatus::raw_key_value://原始键值
				{
					if (!ReadUntil(strKeyValue, fRead, UntilRawKeyNext, true))
					{
						break;//错误处理
					}
				}
				continue;
			case ReadStatus::line_end://行尾
				{
					if (enLastStatus == ReadStatus::key_value || enLastStatus == ReadStatus::raw_key_next)
					{
						csKeyList[strKeyName] = std::move(strKeyValue);
					}
					else if (enLastStatus == ReadStatus::raw_key_value)
					{
						strKeyValue.push_back('\n');//插入被转换的换行符
						enStatus = enLastStatus;//恢复遇到换行符前处理的状态
						continue;//处理下一个字符
					}
					else if (enLastStatus == ReadStatus::section_next)
					{
						KeyList &csLastKeyList = csSecList[strLastSecName];

						csLastKeyList.merge(csKeyList);//合并
						for (auto &[strMergeKeyName, strMergeKeyValue] : csKeyList)//把剩余无法合并入的元素强制赋值
						{
							csLastKeyList[strMergeKeyName] = std::move(strMergeKeyValue);
						}
						csKeyList.clear();//清理

						strLastSecName = strSecName;
					}//else：非正常路径遇到换行，与err相同处理

					//回退字符
					ungetc('\n', fRead);
				}
				break;//进入错误处理
			case ReadStatus::comment://注释
				{
					JumpIf(fRead, IfNextLine, true);//从当前位置到换行符前的内容全部跳过，并把末尾换行符放回字符流以便后续处理行尾
					enStatus = enLastStatus;//恢复遇到注释前处理的状态
				}
				continue;//处理下一个字符
			case ReadStatus::error://错误
				break;//进入错误处理
			}

			//清空字符串
			strSecName.clear();
			strKeyName.clear();
			strKeyValue.clear();

			if (!JumpIf(fRead, IfNextLine, false) ||//跳过这一行
				!JumpIf(fRead, IfNextNoSpace, true))//忽略直到下一个非空字符
			{
				break;
			}
			enStatus = ReadStatus::new_line;
			continue;//处理下一个字符
		}

		KeyList &csLastKeyList = csSecList[strLastSecName];//如果strLastSecName所属的节为空，则自动构造空节
		csLastKeyList.merge(csKeyList);//合并
		for (auto &[strMergeKeyName, strMergeKeyValue] : csKeyList)//把剩余无法合并入的元素强制赋值
		{
			csLastKeyList[strMergeKeyName] = std::move(strMergeKeyValue);
		}
		csKeyList.clear();//清理

		return fclose(fRead) != EOF;
	}

	/// <summary>
	/// 写入配置到ini文件中，
	/// 写入将会覆盖目标文件所有内容
	/// 文件名使用"CON"(Windows)、"/dev/console"(Linux)则写入到标准输出
	/// </summary>
	/// <param name="strFileName">文件名</param>
	/// <returns>写入是否成功</returns>
	bool Write(const FileName &strFileName, bool bSectionNewLine = true)
	{
		FILE *fWrite = fopen(strFileName.c_str(), "w");//打开只写文件并清空
		if (fWrite == NULL)
		{
			return false;
		}

		for (auto &[strSecName, csKeyList] : csSecList)
		{
			if (fprintf(fWrite, "[%s]\n", strSecName.c_str()) == EOF)
			{
				return false;
			}

			for (auto &[strKeyName, strKeyValue] : csKeyList)
			{
				bool bIsRawValue = false;//写入之前判断strKeyValue是否存在空字符，如果是则在前后输出"包围原始值
				for (auto &it : strKeyValue)
				{
					if (isspace((unsigned char)it))
					{
						bIsRawValue = true;
						break;
					}
				}

				if (fprintf(fWrite, "%s=%s", strKeyName.c_str(), bIsRawValue ? "\"" : "") == EOF ||
					fprintf(fWrite, "%s%s\n", strKeyValue.c_str(), bIsRawValue ? "\"" : "") == EOF)
				{
					return false;
				}
			}

			if (bSectionNewLine)
			{
				fputc('\n', fWrite);
			}
		}

		return fclose(fWrite) != EOF;
	}

	/// <summary>
	/// 通过节和键名查找对应的值，如果不存在，则查找失败
	/// </summary>
	/// <param name="strSecName">节名称</param>
	/// <param name="strKeyName">键名称</param>
	/// <returns>如果查找成功为值字符串指针，否则为NULL指针</returns>
	KeyValue *FindKeyValue(const SecName &strSecName, const KeyName &strKeyName)
	{
		auto itSrc = csSecList.find(strSecName);
		if (itSrc == csSecList.end())
		{
			return NULL;
		}

		auto itKey = itSrc->second.find(strKeyName);
		if (itKey == itSrc->second.end())
		{
			return NULL;
		}

		return &itKey->second;
	}


	/// <summary>
	/// 在某个节中添加一个键，如果该节不存在，则添加失败，如果该键已存在，则添加失败
	/// </summary>
	/// <param name="strSecName">要添加的键所属的节名</param>
	/// <param name="strKeyName">要添加的键名</param>
	/// <param name="strKeyValue">键名对应的值（可选参数，默认为空字符串）</param>
	/// <returns>添加是否成功</returns>
	bool AddKey(const SecName &strSecName, const KeyName &strKeyName, const KeyValue &strKeyValue = "")
	{
		auto itSrc = csSecList.find(strSecName);
		if (itSrc == csSecList.end())
		{
			return false;//节不存在
		}

		//插入
		return itSrc->second.emplace(strKeyName, strKeyValue).second;
	}

	/// <summary>
	/// 删除某个节中的某个键，如果该节不存在，则删除失败，如果该键不存在，则删除失败
	/// </summary>
	/// <param name="strSecName">要删除的键所属的节名</param>
	/// <param name="strKeyName">要删除的键名</param>
	/// <returns>删除是否成功</returns>
	bool DelKey(const SecName &strSecName, const KeyName &strKeyName)
	{
		auto itSrc = csSecList.find(strSecName);
		if (itSrc == csSecList.end())
		{
			return false;//节不存在
		}

		return itSrc->second.erase(strKeyName) == 1;//删除的元素数为1则删除成功，否则不存在该元素，
	}

	/// <summary>
	/// 添加一个空节,如果该节已存在，则添加失败
	/// </summary>
	/// <param name="strSecName">要添加的节名</param>
	/// <returns>添加是否成功</returns>
	bool AddSec(const SecName &strSecName)
	{
		return csSecList.emplace(strSecName, KeyList{}).second;
	}

	/// <summary>
	/// 删除一个空节，如果该节不存在，则删除失败
	/// </summary>
	/// <param name="strSecName">要删除的节名</param>
	/// <returns>删除是否成功</returns>
	bool DelSec(const SecName &strSecName)
	{
		return csSecList.erase(strSecName) == 1;
	}

};



