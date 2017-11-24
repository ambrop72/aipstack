
#include <stddef.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/infra/Buf.h>

using namespace AIpStack;

static constexpr Modulo Mod = Modulo(10);

void test_with_offset(size_t off)
{
    char buffer[Mod.modulus()];
    for (size_t i = 0; i < Mod.modulus(); i++) {
        buffer[Mod.add(off, i)] = '0' + i;
    }

    IpBufNode node = {buffer, Mod.modulus(), &node};
    IpBufRef all = {&node, off, Mod.modulus()};

    IpBufRef ref1 = all;
    AIPSTACK_ASSERT_FORCE(ref1.findByte(4, '4') == false);
    AIPSTACK_ASSERT_FORCE(ref1.offset == Mod.add(off, 4));
    
    IpBufRef ref2 = all;
    AIPSTACK_ASSERT_FORCE(ref2.findByte(5, '4') == true);
    AIPSTACK_ASSERT_FORCE(ref2.offset == Mod.add(off, 5));
    
    IpBufRef ref3 = all;
    AIPSTACK_ASSERT_FORCE(ref3.findByte(6, '4') == true);
    AIPSTACK_ASSERT_FORCE(ref3.offset == Mod.add(off, 5));
    
    IpBufRef ref4 = all;
    AIPSTACK_ASSERT_FORCE(ref4.findByte(10, '9') == true);
    AIPSTACK_ASSERT_FORCE(ref4.offset == Mod.add(off, 0));
    
    IpBufRef ref5 = all;
    AIPSTACK_ASSERT_FORCE(ref5.findByte(1, '0') == true);
    AIPSTACK_ASSERT_FORCE(ref5.offset == Mod.add(off, 1));
    
    IpBufRef ref6 = all;
    AIPSTACK_ASSERT_FORCE(ref6.findByte(0, '0') == false);
    AIPSTACK_ASSERT_FORCE(ref6.offset == Mod.add(off, 0));
}

int main (int argc, char *argv[])
{
    for (size_t i = 0; i < Mod.modulus(); i++) {
        test_with_offset(i);
    }
    return 0;
}
