/*
 * The reporter's sample from issue #1102, verbatim. Break on the marked line
 * and plot `baseMember`. Wider shapes live in testbench/realtypes.cpp.
 */
#include <Eigen/Core>

struct Base {
	Eigen::Vector3d baseMember;
};

struct Test : public Base {
	void f() {
		Eigen::Vector3d local;
		// Break here.
	}

	Eigen::Vector3d member;
};

int main()
{
	Test().f();
}
