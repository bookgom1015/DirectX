
namespace Util {
#define ASSERT_EXPR(exp)

	template<typename T>
	void AssertExpr(T exp) {
		std::stringstream sstream;
		std::string str;

		sstream << "(LINE: " << __LINE__ << ") mInput is invalid";
		str = sstream.str();

		std::wstring wstr(str.begin(), str.end());
		_ASSERT_EXPR(exp != nullptr, wstr);
	}
}