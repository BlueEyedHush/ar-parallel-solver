#include <iostream>

using ArrSize = size_t;
using NumType = double;

const ArrSize N = 1000;

class Workspace {
public:
	Workspace(const ArrSize size) : size(size) {
		front = new NumType[size*size];
		back = new NumType[size*size];
	}

	~Workspace() {
		delete[] front;
		delete[] back;
	}

	/**
	 * @return reference to (i,j)th front buffer element
	 */
	inline NumType& elf(const ArrSize i, const ArrSize j) {
		return front[size*i+j];
	}

	/**
	 * @return reference to (i,j)th back buffer element
	 */
	inline NumType& elb(const ArrSize i, const ArrSize j) {
		return back[size*i+j];
	}

	void swap() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

private:
	const ArrSize size;
	NumType *front;
	NumType *back;

};

int main() {
	std::cout << "Sequential variant" << std::endl;

	Workspace w(N);

	return 0;
}