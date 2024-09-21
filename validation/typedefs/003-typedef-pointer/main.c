typedef int *ptr, **ptr_ptr;

int main() {
    int a = 77;
    ptr b = &a;
    ptr_ptr c = &b;
    return **c;
}
