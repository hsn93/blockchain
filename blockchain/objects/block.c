//
// ==================================
// blockchain
//
// an open blockchain network for anything.
// ==================================
//
// Block.c
//
// Eric Meehan
// 4/10/21
//
//

#include "block.h"
#include "../database/database.h"

static bool BLOCKCHAIN_OBJ_PRIVATE_Block_timestamp(BLOCKCHAIN_OBJ_Block *block);
static bool BLOCKCHAIN_OBJ_PRIVATE_Block_sign(BLOCKCHAIN_OBJ_Account *user, BLOCKCHAIN_OBJ_Block *block);
static bool BLOCKCHAIN_OBJ_PRIVATE_Block_validate_signature(BLOCKCHAIN_OBJ_Block *block);
static bool BLOCKCHAIN_OBJ_PRIVATE_Block_validate_hash(BLOCKCHAIN_OBJ_Block *block);

bool BLOCKCHAIN_OBJ_Block_mine(BLOCKCHAIN_OBJ_Account *user, BLOCKCHAIN_OBJ_Block *previous, void *data, unsigned long size, byte *digest)
{
    // Allocate space for the block
    unsigned long block_size = size + sizeof(BLOCKCHAIN_OBJ_BlockHeaders);
    BLOCKCHAIN_OBJ_Block *block = malloc(block_size);
    
    // Initialize some of the fields
    block->headers.nonce = 0;
    if (!BLOCKCHAIN_OBJ_PRIVATE_Block_timestamp(block))
    {
        return false;
    }
    
    // Insert the incidentals and data
    block->headers.size = block_size;
    memcpy(&block->data, data, size);
    //memcpy(block->headers.incidentals.previous_hash, previous, size)
    
    if (!BLOCKCHAIN_OBJ_PRIVATE_Block_sign(user, block))
    {
        return false;
    }
    
    while (!BLOCKCHAIN_OBJ_PRIVATE_Block_validate_hash(block))
    {
        block->headers.nonce += 1;
    }
    
    if (!BLOCKCHAIN_OBJ_PRIVATE_Block_validate_hash(block))
    {
        return false;
    }
    if (!BLOCKCHAIN_OBJ_Block_save(block))
    {
        return false;
    }
    
    return true;
}

bool BLOCKCHAIN_OBJ_Block_validate(BLOCKCHAIN_OBJ_Block *block)
{
    return BLOCKCHAIN_OBJ_PRIVATE_Block_validate_hash(block) && BLOCKCHAIN_OBJ_PRIVATE_Block_validate_signature(block);
}

bool BLOCKCHAIN_OBJ_Block_hash(BLOCKCHAIN_OBJ_Block *block, byte *digest)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), NULL) <= 0)
    {
        return false;
    }
    if (EVP_DigestUpdate(ctx, block, block->headers.size) <= 0)
    {
        return false;
    }
    unsigned int size = 64;
    if (EVP_DigestFinal(ctx, digest, &size) <= 0)
    {
        return false;
    }
    EVP_MD_CTX_free(ctx);
    return true;
}




bool BLOCKCHAIN_OBJ_Block_load(BLOCKCHAIN_OBJ_Block *block, byte *address)
{
    char path[512] = {0};
    BLOCKCHAIN_OBJ_Block_get_path(block, path);
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return false;
    }
    
    block = malloc(sizeof(BLOCKCHAIN_OBJ_BlockHeaders));
    fread(block, sizeof(BLOCKCHAIN_OBJ_BlockHeaders), 1, f);
    unsigned long size = block->headers.size;
    free(block);
    
    block = malloc(size);
    fseek(f, 0, SEEK_SET);
    fread(block, size, 1, f);
    
    fclose(f);
    return true;
}

bool BLOCKCHAIN_OBJ_Block_save(BLOCKCHAIN_OBJ_Block *block)
{
    
    char path[256] = {0};
    BLOCKCHAIN_OBJ_Block_get_path(block, path);
    
    FILE *f = fopen(path, "r");
    if (f)
    {
        fprintf(stdout, "BLOCKCHAIN MESSAGE: Block %s already exists\n", path);
        return false;
    }
    fclose(f);
    f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "BLOCKCHAIN ERROR: Failed to load block %s\n", path);
        return false;
    }
    
    BLOCKCHAIN_DB_insert_block(block);
    fwrite(block, block->headers.size, 1, f);
    fclose(f);
    return true;
}

void BLOCKCHAIN_OBJ_Block_get_path(BLOCKCHAIN_OBJ_Block *block, char *path)
{
    char p[256] = {0};
    strcat(p, BLOCK_PATH);
    strcat(p, "%0128s.block");
    
    byte digest[64] = {0};
    BLOCKCHAIN_OBJ_Block_hash(block, digest);
    
    BIGNUM *hash = BN_new();
    BN_bin2bn(digest, 64, hash);
    char *hex = BN_bn2hex(hash);
    
    sprintf(path, p, hex);
}



bool BLOCKCHAIN_OBJ_PRIVATE_Block_timestamp(BLOCKCHAIN_OBJ_Block *block)
{
    struct tm *raw_time;
    time_t time_object = time(NULL);
    raw_time = gmtime(&time_object);
    char timestamp[20] = {0};
    sprintf(timestamp, "%d-%02d-%02d %02d:%02d:%02d", raw_time->tm_year + 1900, raw_time->tm_mon + 1, raw_time->tm_mday, raw_time->tm_hour, raw_time->tm_min, raw_time->tm_sec);
    memcpy(block->headers.timestamp, timestamp, 20);
    return true;
}

bool BLOCKCHAIN_OBJ_PRIVATE_Block_sign(BLOCKCHAIN_OBJ_Account *user, BLOCKCHAIN_OBJ_Block *block)
{
    // Embed the active user's public key in the block.
    memset(&block->headers.key, 0, 550);
    i2d_PUBKEY(user->public_key, (unsigned char **)&block->headers.key);
    // Collect the size of the data embedded within the block.
    unsigned long long size = block->headers.size - sizeof(BLOCKCHAIN_OBJ_BlockHeaders);
    // Prepare an EVP control structure
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    // Create a signature for the block, checking for errors.
    if ((EVP_DigestSignInit(ctx, NULL, EVP_sha512(), NULL, user->private_key)) <= 0)
    {
        return false;
    }
    if (EVP_DigestSignUpdate(ctx, &block->data, size) <= 0)
    {
        return false;
    }
    size_t len;
    if (EVP_DigestSignFinal(ctx, (unsigned char *)&block->headers.lock, &len) <= 0)
    {
        return false;
    }
    EVP_MD_CTX_free(ctx);
    return true;
}


bool BLOCKCHAIN_OBJ_PRIVATE_Block_validate_signature(BLOCKCHAIN_OBJ_Block *block)
{
    // Prepare an EVP control structure.
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    // Load the embedded public key as an EVP_PKEY.
    EVP_PKEY *key = EVP_PKEY_new();
    d2i_PUBKEY(&key, (const unsigned char **)&block->headers.key, 550);
    // Verify the signature, checking for errors.
    
    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha512(), NULL, key) <= 0)
    {
        return false;
    }
    if (EVP_DigestVerifyUpdate(ctx, &block->data, block->headers.size - sizeof(BLOCKCHAIN_OBJ_BlockHeaders)) <= 0)
    {
        return false;
    }
    if (EVP_DigestVerifyFinal(ctx, block->headers.lock, 512) <= 0)
    {
        return false;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return true;
}

bool BLOCKCHAIN_OBJ_PRIVATE_Block_validate_hash(BLOCKCHAIN_OBJ_Block *block)
{
    // Create a new bignum control structure
    BN_CTX *ctx = BN_CTX_new();
    
    // The target hash value is 2^(512-difficulty).
    // This needs to be represented using the OpenSSL bignum library.
    BIGNUM *base = BN_new();
    BN_dec2bn(&base, "2");
    
    BIGNUM *exponent = BN_new();
    // Truncate the difficulty to an intiger to avoid contentious precision.
    int difficulty = 512 - pow(log(sqrt(block->headers.size)), 2) + 1;
    char exp[8];
    sprintf(exp, "%df", difficulty);
    BN_dec2bn(&exponent, exp);
    
    BIGNUM *target = BN_new();
    BN_exp(target, base, exponent, ctx);
    
    BIGNUM *hash = BN_new();
    byte digest[64];
    BLOCKCHAIN_OBJ_Block_hash(block, digest);
    BN_bin2bn(digest, 64, hash);
    
    // If the block is valid, its hash will be less than the target.
    int proof_of_work = BN_cmp(target, hash);
    
    // Clean up after OpenSSL
    BN_free(base);
    BN_free(exponent);
    BN_free(target);
    BN_free(hash);
    BN_CTX_free(ctx);
    
    return proof_of_work == 1;
}
