//
// ==================================
// blockchain
//
// an open blockchain network for anything.
// ==================================
//
// Account.c
//
// Eric Meehan
// 5/2/21
//
//

#include "account.h"

bool BLOCKCHAIN_OBJ_Account_create(BLOCKCHAIN_OBJ_Account *user, char *name)
{
    EVP_PKEY *key = EVP_PKEY_new();
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 4096);
    EVP_PKEY_keygen(ctx, &key);
    
    char path[64] = {0};
    strcat(path, PROFILE_PATH);
    strcat(path, name);
    strcat(path, ".pem");
    
    FILE *key_file = fopen(path, "r");
    if (key_file)
    {
        return false;
    }
    fclose(key_file);
    
    key_file = fopen(path, "w");
    if (!PEM_write_PrivateKey(key_file, key, EVP_aes_256_cbc(), NULL, 0, NULL, NULL))
    {
        fprintf(stderr, "BLOCKCHAIN ERROR: Failed to write private key for %s\n", name);
        return EXIT_FAILURE;
    }
    fclose(key_file);
    
    memset(path, 0, 64);
    strcat(path, PROFILE_PATH);
    strcat(path, name);
    strcat(path, ".der");
    
    key_file = fopen(path, "w");
    if (!i2d_PUBKEY_fp(key_file, key))
    {
        fprintf(stderr, "BLOCKCHAIN ERROR: Failed to write public key for %s\n", name);
        return EXIT_FAILURE;
    }
    fclose(key_file);
    
    EVP_PKEY_CTX_free(ctx);
    
    return true;
}

bool BLOCKCHAIN_OBJ_Account_activate(BLOCKCHAIN_OBJ_Account *user, char *name)
{
    char path[64] = {0};
    strcat(path, PROFILE_PATH);
    strcat(path, name);
    strcat(path, ".der");
    FILE *key_file = fopen(path, "r");
    if (!key_file)
    {
        return false;
    }
    user->public_key = EVP_PKEY_new();
    d2i_PUBKEY_fp(key_file, &user->public_key);
    fclose(key_file);
    
    memset(path, 0, 64);
    strcat(path, PROFILE_PATH);
    strcat(path, name);
    strcat(path, ".pem");
    key_file = fopen(path, "r");
    if (!key_file)
    {
        return false;
    }
    user->private_key = EVP_PKEY_new();
    user->private_key = PEM_read_PrivateKey(key_file, &user->private_key, NULL, NULL);
    fclose(key_file);
    
    return user->public_key && user->private_key;
}

bool BLOCKCHAIN_OBJ_Account_deactivate(BLOCKCHAIN_OBJ_Account *user)
{
    
    user->public_key = NULL;
    user->private_key = NULL;
    
    return !user->public_key && !user->private_key;
}

void BLOCKCHAIN_OBJ_Account_print(BLOCKCHAIN_OBJ_Account *user)
{
    PEM_write_PUBKEY(stdout, user->public_key);
}

bool BLOCKCHAIN_OBJ_Account_login(BLOCKCHAIN_OBJ_Account *user)
{
    fprintf(stdout, "Username: ");
    char name[256] = {0};
    fgets(name, 256, stdin);
    for (int i = 0; i < strlen(name); i++)
    {
        if (name[i] == '\n')
        {
            name[i] = '\0';
        }
    }
    return BLOCKCHAIN_OBJ_Account_activate(user, name);
}
