#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>

void md5_hash_string(const unsigned char *message, size_t count, unsigned char out[5]) {
    unsigned char hash[MD5_DIGEST_LENGTH];    
    MD5(message, count, hash);
    memcpy(out, hash, 5*sizeof(unsigned char));
}

int main(void) {
    unsigned char hash[5] = {0};
    const char password[] = "password";
    md5_hash_string((const unsigned char *) password, sizeof(password), hash);
    for (size_t i = 0; i < 5; i++) {
        printf("%x", hash[i]);
    }
    printf("\n");
    return 0;
}
