//
// Created by blueeyedhush on 31.10.17.
//

#ifndef LAB1_NONCOPYABLE_H
#define LAB1_NONCOPYABLE_H

class NonCopyable
{
protected:
	NonCopyable () {}
	~NonCopyable () {} /// Protected non-virtual destructor
private:
	NonCopyable (const NonCopyable &) = delete;
	NonCopyable & operator=(const NonCopyable &) = delete;
};

#endif //LAB1_NONCOPYABLE_H
