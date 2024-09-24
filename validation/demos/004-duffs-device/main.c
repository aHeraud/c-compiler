int printf(const char *fmt, ...);

// copy count integers from *to to *from
// manual loop unrolling by combining a switch statement and a loop
// see: https://en.wikipedia.org/wiki/Duff%27s_device
void send(int *to, int *from, int count) {
    int n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *to++ = *from++;
        case 7:      *to++ = *from++;
        case 6:      *to++ = *from++;
        case 5:      *to++ = *from++;
        case 4:      *to++ = *from++;
        case 3:      *to++ = *from++;
        case 2:      *to++ = *from++;
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
}

int main(int argc, char **argv) {
    int from[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };
    int to[20];
    send(to, from, 20);

    for (int i = 0; i < 20; i += 1) {
        printf("%d\n", to[i]);
    }
}
