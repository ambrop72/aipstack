
#include <cstddef>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/infra/Buf.h>

using namespace AIpStack;

static constexpr Modulo Mod = Modulo(10);

void test_with_offset(std::size_t off)
{
    char buffer[Mod.modulus()];
    for (std::size_t i = 0; i < Mod.modulus(); i++) {
        buffer[Mod.add(off, i)] = '0' + i;
    }

    IpBufNode node = {buffer, Mod.modulus(), &node};
    IpBufRef all = {&node, off, Mod.modulus()};

    // findByte tests

    IpBufRef ref1 = all;
    AIPSTACK_ASSERT_FORCE(ref1.findByte('4', 4) == false);
    AIPSTACK_ASSERT_FORCE(ref1.offset == Mod.add(off, 4));
    
    IpBufRef ref2 = all;
    AIPSTACK_ASSERT_FORCE(ref2.findByte('4', 5) == true);
    AIPSTACK_ASSERT_FORCE(ref2.offset == Mod.add(off, 5));
    
    IpBufRef ref3 = all;
    AIPSTACK_ASSERT_FORCE(ref3.findByte('4', 6) == true);
    AIPSTACK_ASSERT_FORCE(ref3.offset == Mod.add(off, 5));
    
    IpBufRef ref4 = all;
    AIPSTACK_ASSERT_FORCE(ref4.findByte('9', 10) == true);
    AIPSTACK_ASSERT_FORCE(ref4.offset == Mod.add(off, 0));
    
    IpBufRef ref5 = all;
    AIPSTACK_ASSERT_FORCE(ref5.findByte('0', 1) == true);
    AIPSTACK_ASSERT_FORCE(ref5.offset == Mod.add(off, 1));
    
    IpBufRef ref6 = all;
    AIPSTACK_ASSERT_FORCE(ref6.findByte('0', 0) == false);
    AIPSTACK_ASSERT_FORCE(ref6.offset == Mod.add(off, 0));
    
    IpBufRef ref7 = all;
    AIPSTACK_ASSERT_FORCE(ref7.findByte('A', Mod.modulus() + 1) == false);
    AIPSTACK_ASSERT_FORCE(ref7.offset == Mod.add(off, 0));

    // startsWith tests

    IpBufRef rem1;
    AIPSTACK_ASSERT_FORCE(all.startsWith("0", rem1) == true)
    AIPSTACK_ASSERT_FORCE(rem1.offset == Mod.add(off, 1))
    AIPSTACK_ASSERT_FORCE(rem1.tot_len == Mod.modulus() - 1)

    IpBufRef rem2;
    AIPSTACK_ASSERT_FORCE(all.startsWith("0123", rem2) == true)
    AIPSTACK_ASSERT_FORCE(rem2.offset == Mod.add(off, 4))
    AIPSTACK_ASSERT_FORCE(rem2.tot_len == Mod.modulus() - 4)

    IpBufRef rem3;
    AIPSTACK_ASSERT_FORCE(all.startsWith("0123456789", rem3) == true)
    AIPSTACK_ASSERT_FORCE(rem3.offset == Mod.add(off, 10))
    AIPSTACK_ASSERT_FORCE(rem3.tot_len == Mod.modulus() - 10)

    IpBufRef rem4;
    AIPSTACK_ASSERT_FORCE(all.startsWith("01234567890", rem4) == false)

    IpBufRef rem5;
    AIPSTACK_ASSERT_FORCE(all.startsWith("0123456X", rem5) == false)

    IpBufRef rem6;
    AIPSTACK_ASSERT_FORCE(all.startsWith("X123456", rem6) == false)

    IpBufRef rem7;
    AIPSTACK_ASSERT_FORCE(all.startsWith(AIpStack::MemRef(), rem7) == true)
    AIPSTACK_ASSERT_FORCE(rem7.offset == all.offset)
    AIPSTACK_ASSERT_FORCE(rem7.tot_len == all.tot_len)
}

int main (int argc, char *argv[])
{
    for (std::size_t i = 0; i < Mod.modulus(); i++) {
        test_with_offset(i);
    }
    return 0;
}
