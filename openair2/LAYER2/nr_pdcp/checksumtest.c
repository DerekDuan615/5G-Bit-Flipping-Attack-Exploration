#include <stdio.h>
#include <stdlib.h>

/* compile:
   gcc test.c -o test -lz3
*/
/*++
Copyright (c) 2015 Microsoft Corporation

--*/

#include <stdio.h>

unsigned short checksum(unsigned short *ptr, int length)
{
    int sum = 0;
    unsigned short answer = 0;
    unsigned short *w = ptr;
    int nleft = length;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}

int main()
{
    // unsigned char data[] = {
    //     0x0a, 0x00, 0x00, 0x02, 0xc0, 0xa8, 0x46, 0x87,
    //     0x00, 0x11, 0x00, 0x2c, 0xad, 0xcf, 0x04, 0xd2,
    //     0x00, 0x2c, 0x00, 0x00, 0x7b, 0x22, 0x6b, 0x65,
    //     0x79, 0x31, 0x22, 0x3a, 0x20, 0x22, 0x76, 0x61,
    //     0x6c, 0x75, 0x65, 0x31, 0x22, 0x2c, 0x20, 0x22,
    //     0x6b, 0x65, 0x79, 0x32, 0x22, 0x3a, 0x20, 0x22,
    //     0x76, 0x61, 0x6c, 0x75, 0x65, 0x32, 0x22, 0x7d};

    // unsigned char data[] = {
    //     0x0a, 0x00, 0x00, 0x02, 0xc0, 0xa8, 0x46, 0x87,
    //     0x00, 0x11, 0x00, 0x2c, 0xad, 0xcf, 0x04, 0xd2,
    //     0x00, 0x2c, 0x00, 0x00, 0x7b, 0x22, 0x6b, 0x67,
    //     0x79, 0x31, 0x22, 0x3a, 0x20, 0x22, 0x76, 0x61,
    //     0x6c, 0x75, 0x65, 0x31, 0x22, 0x2c, 0x20, 0x22,
    //     0x6b, 0x65, 0x79, 0x32, 0x22, 0x3a, 0x20, 0x22,
    //     0x76, 0x61, 0x6c, 0x75, 0x65, 0x32, 0x22, 0x7d};

    // unsigned char data[] = {
    //     0x0a, 0x00, 0x00, 0x02, 0xc0, 0xa8, 0x46, 0x87,
    //     0x00, 0x11, 0x00, 0x13, 0xad, 0xcf, 0x04, 0xd2,
    //     0x00, 0x13, 0x00, 0x00, 0x22, 0x70, 0x33, 0x30,
    //     0x30, 0x76, 0x32, 0x35, 0x61, 0x32, 0x22, 0x00};

    unsigned char data[] = {
        0x30, 0x36, 0x76, 0x32, 0x35, 0x61, 0x32, 0x22};

    // unsigned char data[] = {
    //     0x0a, 0x00, 0x00, 0x02, 0xc0, 0xa8, 0x46, 0x87};

    int length = sizeof(data);
    unsigned short result = checksum((unsigned short *)data, length);
    // we need to flip the bytes because when we call the checksum function,
    // the parameter "unsigned short *ptr" read the "unsigned char data[]" regard it as little-endian.
    // Therefore, the data[] will store in the memory from low address to high address as {0x0a, 0x00, 0x00, 0x02, 0xc0, 0xa8, 0x46, 0x87}
    // but because of little-endian of my machine, when we call the checksum function,
    // it will read the data[] as "0x000a", "0x0200", "0xa8c0", "0x8746"
    // Therefore, in this program , we need to flip the bytes
    // However, OAI use big-endian, so we do not consider flip the bytes there.
    printf("Checksum: 0x%04x\n", ((result & 0xFF) << 8) | (result >> 8));
    printf("Size of int: %zu bytes\n", sizeof(int));
    printf("Size of unsigned short: %zu bytes\n", sizeof(unsigned short));
    printf("Size of char: %zu bytes\n", sizeof(unsigned char));

    return 0;
}

/* Example Explained (No overflow)

1. The input data is

unsigned char data[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06
};

2. This checksum function adds data as a unit of 2 bytes, not 1 byte

data = [0x0102, 0x0304, 0x0506]

Iteration 1:
sum = 0 + 0x0102 = 0x0102    (Binary: 0000 0001 0000 0010)
nleft = 6 - 2 = 4

Iteration 2:
sum = 0x0102 + 0x0304 = 0x0406    (Binary: 0000 0100 0000 0110)
nleft = 4 - 2 = 2

Iteration 3:
sum = 0x0406 + 0x0506 = 0x090C    (Binary: 0000 1001 0000 1100)
nleft = 2 - 2 = 0

3. Handle Overflow:
No overflow in this example, so folding is not needed here.

4. One’s Complement:

answer = ~sum
answer = ~0x090C = 0xF6F3    (Binary: 1111 0110 1111 0011)

5. Output:

Checksum: 0xf3f6

*/

/* Example Explained (Overflow)

1. The input data is:

unsigned char data[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

2. This checksum function adds data as a unit of 2 bytes, not 1 byte

// data interpreted as 16-bit values: 0xFFFF, 0xFFFF, 0xFFFF

Iteration 1:
sum = 0 + 0xFFFF = 0xFFFF  // Binary: 1111 1111 1111 1111
nleft = 6 - 2 = 4

Iteration 2:
sum = 0xFFFF + 0xFFFF = 0x1FFFE  // Binary: 0001 1111 1111 1111 1110, overflow here
nleft = 4 - 2 = 2

Iteration 3:
sum = 0x1FFFE + 0xFFFF = 0x2FFFD  // Binary: 0010 1111 1111 1111 1101, overflow again
nleft = 2 - 2 = 0

3. After the loop, we need to fold the overflow bits back into the lower 16 bits.

sum = (sum >> 16) + (sum & 0xFFFF);
// sum >> 16: Higher 16 bits, Binary: 0000 0000 0000 0010, Decimal: 2
// sum & 0xFFFF: Lower 16 bits, Binary: 1111 1111 1111 1101, Decimal: 65533

sum = 2 + 65533 = 65535  // Binary: 1111 1111 1111 1111

4. One's complement

answer = ~sum;
// ~0xFFFF = 0x0000  // Binary: 0000 0000 0000 0000

5. Output:

Checksum: 0x0000


*/