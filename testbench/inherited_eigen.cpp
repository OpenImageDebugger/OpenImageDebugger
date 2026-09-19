/*
 * The reporter's sample from issue #1102, kept verbatim.
 *
 * Break on the marked line and plot `baseMember`: held by the base class, it
 * was offered under a name no frame could evaluate and failed to plot, while
 * `local` and `member` worked. Verbatim on purpose -- this is the shape the
 * report described, so it is the one a regression has to be checked against.
 * testbench/realtypes.cpp carries the wider set (a member named after its
 * base, a derived member hiding an inherited one, members reached from
 * inside a method).
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
