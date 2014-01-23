//===-- main.cpp ------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

struct Pair {
	int x;
	int y;
	
	Pair(int _x, int _y) : x(_x), y(_y) {}	
};

int main() {
	Pair p1(3,-3);
	return p1.x + p1.y; // Set break point at this line.
}
