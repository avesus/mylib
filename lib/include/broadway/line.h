#ifndef _LINE_H_
#define _LINE_H_
#include <string>

using namespace std;

struct line {
	unsigned linen;
	string character;
	string msg;

	bool operator<(const line& l) const {
		return linen < l.linen;
	}
};

#endif
