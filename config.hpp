
/***
 * World config
 ***/

/* Will close old file handles */
#define WORLD_MAX_FILE_HANDLES 16
#define WORLD_MAX_CHUNKS_LOADED 2048

/* Negative and positive X and Y range of chunks allowed to be created */
#define WORLD_MAX_CHUNK_XY 0xFFFFF

/* Rate of world updates sent to the client */
#define WORLD_UPDATE_RATE_MSEC 60

/* Maximum value is 255, maximum player updates every WORLD_UPDATE_RATE_MSEC */
/* This is not the player limit */
#define WORLD_MAX_PLAYER_UPDATES 128
#define WORLD_MAX_PLAYER_LEFT_UPDATES 255

/* Maximum value is 65535, max pixel updates every WORLD_UPDATE_RATE_MSEC */
#define WORLD_MAX_PIXEL_UPDATES 4096

/***
 * Client config
 ***/

#define CLIENT_MAX_WARN_LEVEL 128

/* (rate, per n seconds) */
#define CLIENT_PIXEL_UPD_RATELIMIT 32, 4
#define CLIENT_CHAT_RATELIMIT 4, 6
