
#include <iostream>

#include "DX12Game/GameCore.h"

struct TempStruct {
	TempStruct(int _a = 0, int _b = 0) {
		a = _a;
		b = _b;
	}

	int a;
	int b;
};

int main(int argc, char* argv[]) {
	try {
		std::vector<int> vec(5);
		GUnorderedMap<int, int> gmap;
		gmap[0] = 0;
		gmap[1] = 1;

		for (const auto& m : gmap)
			std::cout << "key: " << m.first << '\t' << "value: " << m.second << std::endl;

		 GUnorderedMap<int, int>& refMap = gmap;

		for (const auto& m : refMap) {

		}

		std::cout << "value: " << gmap[32] << std::endl;

		{
			auto iter = gmap.find(0);
			if (iter != gmap.end())
				std::cout << "iter key: " << iter->first << '\t' << "iter value: " << iter->second << std::endl;
		}
		{
			auto iter = gmap.find(1);
			if (iter != gmap.end())
				std::cout << "iter key: " << iter->first << '\t' << "iter value: " << iter->second << std::endl;
		}
		{
			auto iter = gmap.find(2);
			if (iter != gmap.end())
				std::cout << "iter key: " << iter->first << '\t' << "iter value: " << iter->second << std::endl;
		}
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}

	system("pause");
	return 0;
}