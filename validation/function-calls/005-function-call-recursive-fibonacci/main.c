int fib(int n);

int fib(int n) {
    if (n  < 2) {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

int main() {
    return fib(10);
}
