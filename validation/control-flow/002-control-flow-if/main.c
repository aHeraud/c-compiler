int main(int argc, char** argv) {
    int result = 1;
    if (argc == 0) {
        result = 1;
    }

    if (argc == 1) {
        result = 0;
    }

    return result;
}
