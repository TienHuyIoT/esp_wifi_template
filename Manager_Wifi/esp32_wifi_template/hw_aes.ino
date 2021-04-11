#include <aes/esp_aes.h>

#define CRYPTO_PORT Serial
#define CRYPTO_PRINTF(f_, ...) CRYPTO_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void aes_encrypt_ecb(const unsigned char *key, const unsigned char *input, uint8_t *output)
{
    CRYPTO_PRINTF("\r\nESP_AES_ENCRYPT");
    aes_crypt_ecb(key, input, output, ESP_AES_ENCRYPT);
}
void aes_decrypt_ecb(const unsigned char *key, const unsigned char *input, uint8_t *output)
{
    CRYPTO_PRINTF("\r\nESP_AES_DECRYPT");
    aes_crypt_ecb(key, input, output, ESP_AES_DECRYPT);
}

void aes_crypt_ecb(const unsigned char *key, const unsigned char *input, uint8_t *output, uint8_t mode)
{
    esp_aes_context aes;
    char hex_str[32];
    esp_aes_init(&aes);
    /**
     * \brief          AES set key schedule (encryption or decryption)
     *
     * \param ctx      AES context to be initialized
     * \param key      encryption key
     * \param keybits  must be 128, 192 or 256
     *
     * \return         0 if successful, or ERR_AES_INVALID_KEY_LENGTH
     * int esp_aes_setkey( esp_aes_context *ctx, const unsigned char *key, unsigned int keybits );
     */
    esp_aes_setkey(&aes, (const unsigned char *)key, 16 * 8);

    /**
     * \brief          AES-ECB block encryption/decryption
     *
     * \param ctx      AES context
     * \param mode     ESP_AES_ENCRYPT or ESP_AES_DECRYPT
     * \param input    16-byte input block
     * \param output   16-byte output block
     *
     * \return         0 if successful
     * int esp_aes_crypt_ecb( esp_aes_context *ctx, int mode, const unsigned char input[16], unsigned char output[16] );
     */
    esp_aes_crypt_ecb(&aes, mode, (const unsigned char *)input, output);
    esp_aes_free(&aes);

    hex2str(hex_str, (uint8_t*)key, 16, UPPER_CASE);
    CRYPTO_PRINTF("\r\nAes key: %s", hex_str);
    hex2str(hex_str, (uint8_t*)input, 16, UPPER_CASE);
    CRYPTO_PRINTF("\r\nAes input: %s", hex_str);
    hex2str(hex_str, (uint8_t*)output, 16, UPPER_CASE);
    CRYPTO_PRINTF("\r\nAes output: %s\r\n", hex_str);
}