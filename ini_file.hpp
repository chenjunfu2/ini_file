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
		new_line,//����
		section_name,//����
		section_next,//�ں�
		key_name,//����
		key_value,//��ֵ
		line_end,//��β
		comment,//ע��
		error,//����
	};

	//����ֱ��ν�ʷ���true
	template<typename T>
	static bool JumpIf(FILE *f, T t, bool unget = true)//tΪһԪν��
	{
		int c;//���ﲻʹ��char��ԭ����Ϊ���ж�EOF�����������ļ��ڴ���0xffҲ��Ӱ��EOF���ж�
		while ((c = fgetc(f)) != EOF)
		{
			if (t(c))
			{
				if (unget == true)
				{
					ungetc(c, f);//����
				}
				return true;
			}
		}

		return false;//�����ļ�β
	}

	static bool IfNextNoSpace(int c)//��һ���ǿ��ַ�����Խ����
	{
		return !isspace(c);
	}

	static bool IfNextLine(int c)//��һ��
	{
		return c == '\n';
	}

	//����true��continueһ��
	static bool StatusTransition(ReadStatus& enStatus, int c)
	{
		switch (c)
		{
		case '['://����
			{
				if (enStatus != ReadStatus::new_line)//����
				{
					enStatus = ReadStatus::error;
					break;
				}
				enStatus = ReadStatus::section_name;
				return true;
			}
			break;
		case ']'://�ں�
			{
				if (enStatus != ReadStatus::section_name)//����
				{
					enStatus = ReadStatus::error;
					break;
				}
				enStatus = ReadStatus::section_next;
				return true;
			}
			break;
		case ';'://ע��
			{
				enStatus = ReadStatus::comment;
			}
			break;
		case '='://��ֵ
			{
				if (enStatus != ReadStatus::key_name)//����
				{
					enStatus = ReadStatus::error;
					break;
				}
				enStatus = ReadStatus::key_value;
				return true;
			}
			break;
		case '\n'://��β
			{
				enStatus = ReadStatus::line_end;
			}
			break;
		default:
			{
				if (enStatus == ReadStatus::new_line)//����
				{
					enStatus = ReadStatus::key_name;//����
				}
			}
			break;
		}

		return false;
	}

public:
	ini_file(void) :csSecList({{"",KeyList{}}})//Ĭ�ϴ���һ���ս�
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
	/// ��ini�ļ��ж�ȡ���ã�
	/// ��������ļ��ڴ��ڷǷ����ݣ������ֱ���Ϸ�����
	/// �ļ���ʹ��"CON"(Windows)��"/dev/console"(Linux)��ӱ�׼�������
	/// </summary>
	/// <param name="strFileName">�ļ���</param>
	/// <returns>��ȡ�Ƿ�ɹ�</returns>
	bool Read(const FileName &strFileName)
	{
		FILE *fRead = fopen(strFileName.c_str(), "r");//��ֻ���ļ�
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
		
		if (!JumpIf(fRead, IfNextNoSpace, true))//����ֱ����һ���ǿ��ַ�
		{
			return true;
		}

		while (true)
		{
			enLastStatus = enStatus;
			if ((c = fgetc(fRead)) == EOF)
			{
				enStatus = ReadStatus::line_end;//EOF�����β����һ�Σ����ڴ�������������bContinueΪfalse
			}
			else if (StatusTransition(enStatus, c))//ת��״̬
			{
				continue;
			}

			switch (enStatus)
			{
			default://���ش�������ʱ���ݴ���
			case ReadStatus::new_line://����
				{
					return fclose(fRead), false;//��������
				}
				break;//���������
			case ReadStatus::section_name://����
				{
					strSecName.push_back(c);
				}
				continue;//������һ���ַ�
			case ReadStatus::section_next://�ں�
				{
					if (isspace(c))
					{
						continue;
					}
				}
				break;//���������
			case ReadStatus::key_name://����
				{
					strKeyName.push_back(c);
				}
				continue;//������һ���ַ�
			case ReadStatus::key_value://��ֵ
				{
					strKeyValue.push_back(c);
				}
				continue;//������һ���ַ�
			case ReadStatus::line_end://��β
				{
					if (enLastStatus == ReadStatus::key_value)
					{
						csKeyList[strKeyName] = std::move(strKeyValue);
					}
					else if (enLastStatus == ReadStatus::section_next)
					{
						KeyList &csLastKeyList = csSecList[strLastSecName];

						csLastKeyList.merge(csKeyList);//�ϲ�
						for (auto &[strMergeKeyName, strMergeKeyValue] : csKeyList)//��ʣ���޷��ϲ����Ԫ��ǿ�Ƹ�ֵ
						{
							csLastKeyList[strMergeKeyName] = std::move(strMergeKeyValue);
						}
						csKeyList.clear();//����

						strLastSecName = strSecName;
					}
					//�����ַ�
					ungetc('\n', fRead);
				}
				break;//���������
			case ReadStatus::comment://ע��
				{
					JumpIf(fRead, IfNextLine, true);//�ӵ�ǰλ�õ����з�ǰ������ȫ������
					enStatus = enLastStatus;//�ָ�����ע��ǰ������״̬
				}
				continue;//������һ���ַ�
			case ReadStatus::error://����
				break;//���������
			}

			//����ַ���
			strSecName.clear();
			strKeyName.clear();
			strKeyValue.clear();

			if (!JumpIf(fRead, IfNextLine, false) ||//������һ��
				!JumpIf(fRead, IfNextNoSpace, true))//����ֱ����һ���ǿ��ַ�
			{
				break;
			}
			enStatus = ReadStatus::new_line;
			continue;//������һ���ַ�
		}

		KeyList &csLastKeyList = csSecList[strLastSecName];//���strLastSecName�����Ľ�Ϊ�գ����Զ�����ս�
		csLastKeyList.merge(csKeyList);//�ϲ�
		for (auto &[strMergeKeyName, strMergeKeyValue] : csKeyList)//��ʣ���޷��ϲ����Ԫ��ǿ�Ƹ�ֵ
		{
			csLastKeyList[strMergeKeyName] = std::move(strMergeKeyValue);
		}
		csKeyList.clear();//����

		return fclose(fRead) != EOF;
	}

	/// <summary>
	/// д�����õ�ini�ļ��У�
	/// д�뽫�Ḳ��Ŀ���ļ���������
	/// �ļ���ʹ��"CON"(Windows)��"/dev/console"(Linux)��д�뵽��׼���
	/// </summary>
	/// <param name="strFileName">�ļ���</param>
	/// <returns>д���Ƿ�ɹ�</returns>
	bool Write(const FileName &strFileName, bool bSectionNewLine = true)
	{
		FILE *fWrite = fopen(strFileName.c_str(), "w");//��ֻд�ļ������
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
				if (fprintf(fWrite, "%s=%s\n", strKeyName.c_str(), strKeyValue.c_str()) == EOF)
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
	/// ͨ���ںͼ������Ҷ�Ӧ��ֵ����������ڣ������ʧ��
	/// </summary>
	/// <param name="strSecName">������</param>
	/// <param name="strKeyName">������</param>
	/// <returns>������ҳɹ�Ϊֵ�ַ���ָ�룬����ΪNULLָ��</returns>
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
	/// ��ĳ����������һ����������ýڲ����ڣ�������ʧ�ܣ�����ü��Ѵ��ڣ�������ʧ��
	/// </summary>
	/// <param name="strSecName">Ҫ���ӵļ������Ľ���</param>
	/// <param name="strKeyName">Ҫ���ӵļ���</param>
	/// <param name="strKeyValue">������Ӧ��ֵ����ѡ������Ĭ��Ϊ���ַ�����</param>
	/// <returns>�����Ƿ�ɹ�</returns>
	bool AddKey(const SecName &strSecName, const KeyName &strKeyName, const KeyValue &strKeyValue = "")
	{
		auto itSrc = csSecList.find(strSecName);
		if (itSrc == csSecList.end())
		{
			return false;//�ڲ�����
		}

		//����
		return itSrc->second.emplace(strKeyName, strKeyValue).second;
	}

	/// <summary>
	/// ɾ��ĳ�����е�ĳ����������ýڲ����ڣ���ɾ��ʧ�ܣ�����ü������ڣ���ɾ��ʧ��
	/// </summary>
	/// <param name="strSecName">Ҫɾ���ļ������Ľ���</param>
	/// <param name="strKeyName">Ҫɾ���ļ���</param>
	/// <returns>ɾ���Ƿ�ɹ�</returns>
	bool DelKey(const SecName &strSecName, const KeyName &strKeyName)
	{
		auto itSrc = csSecList.find(strSecName);
		if (itSrc == csSecList.end())
		{
			return false;//�ڲ�����
		}

		return itSrc->second.erase(strKeyName) == 1;//ɾ����Ԫ����Ϊ1��ɾ���ɹ������򲻴��ڸ�Ԫ�أ�
	}

	/// <summary>
	/// ����һ���ս�,����ý��Ѵ��ڣ�������ʧ��
	/// </summary>
	/// <param name="strSecName">Ҫ���ӵĽ���</param>
	/// <returns>�����Ƿ�ɹ�</returns>
	bool AddSec(const SecName &strSecName)
	{
		return csSecList.emplace(strSecName, KeyList{}).second;
	}

	/// <summary>
	/// ɾ��һ���սڣ�����ýڲ����ڣ���ɾ��ʧ��
	/// </summary>
	/// <param name="strSecName">Ҫɾ���Ľ���</param>
	/// <returns>ɾ���Ƿ�ɹ�</returns>
	bool DelSec(const SecName &strSecName)
	{
		return csSecList.erase(strSecName) == 1;
	}

};


