# ini文件格式说明（来自百度百科）：
节(section)</br>
节用方括号括起来，单独占一行，例如：</br>
\[section\]</br>

键(key)</br>
键(key)又名属性(property)，单独占一行用等号连接键名和键值，例如：</br>
name=value</br>

注释(comment)</br>
注释使用英文分号（;）开头，单独占一行。在分号后面的文字，直到该行结尾都全部为注释，例如：</br>
; comment text</br>

# ini_file类说明：
提供简易方便的接口来操作ini文件，构造类后类默认为空，使用Read成员函数从文件中读取数据，然后使用其它操作函数修改内容，并使用Write成员函数写入文件。</br>
本类不占用文件，仅在Read函数内读入文件并关闭，在Write函数内写出文件并关闭。</br>
如果遇到ini文件中非法部分，则自动跳过，就当没看见（doge）。</br>
