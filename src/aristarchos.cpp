#include <cstring>
#include <cstdio>
#include "andor2k.hpp"
#include "aristarchos.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <thread>
#include <bzlib.h>

using andor2k::ClientSocket;
using andor2k::Socket;

/// @brief Decompress a bzip2 string
/// The function accepts a bzip2 input string and decompresses it into the
/// the provided dest buffer. We can't know beforehand the size of the
/// resulting, decompressed string, so this buffer should be large enough (just
/// how larger a have no clue!). The actual size of the resulting string will be
/// written in destLen.
/// To make sure that the size of dest is large enough, the function expects
/// that it is of size ARISTARCHOS_DECODE_BUFFER_SIZE
/// @param[in] source The input bzip2 compressed string
/// @param[out] dest On success, it will hold the (null-terminated) resulting/
///             decompressed string
/// @param[out] destLen The size of the dest string, aka the size of the 
///             decompressed string
/// @return On sucess, a pointer to dest will be returned; on error, the 
///         function will return a null pointer
/// @see http://linux.math.tifr.res.in/manuals/html/manual_3.html
char *uncompress_bz2_string(const char* source, char *dest, unsigned int& destLen) noexcept {

    std::memset(dest, '\0', ARISTARCHOS_DECODE_BUFFER_SIZE);    
    unsigned int sourceLen = std::strlen(source);
    int error;


    if (error =  BZ2_bzBuffToBuffDecompress(dest, &destLen, source, sourceLen, 0, 0); error == BZ_OK) {
        dest[destLen] = '\0';
        return dest;
    }

    char buf[32];
    switch (error) {
        case BZ_CONFIG_ERROR:
            fprintf(stderr, "[ERROR][%s] decompression error: bzlib library has been mis-compiled! (traceback: %s)\n", date_str(buf), __func__);
            break;
        case BZ_PARAM_ERROR:
            fprintf(stderr, "[ERROR][%s] descompression error: dest is NULL or destLen is NULL! (traceback: %s)\n", date_str(buf), __func__);
            break;
        case BZ_MEM_ERROR:
            fprintf(stderr, "[ERROR][%s] descompression error: insufficient memory is available! (traceback: %s)\n", date_str(buf), __func__);
            break;
        case BZ_OUTBUFF_FULL:
            fprintf(stderr, "[ERROR][%s] the size of the compressed data exceeds *destLen! (traceback: %s)\n", date_str(buf), __func__);
            break;
        case BZ_DATA_ERROR:
            fprintf(stderr, "[ERROR][%s] descompression error: a data integrity error was detected in the compressed data! (traceback: %s)\n", date_str(buf), __func__);
            break;
        case BZ_DATA_ERROR_MAGIC:
            fprintf(stderr, "[ERROR][%s] descompression error: the compressed data doesn't begin with the right magic bytes! (traceback: %s)\n", date_str(buf), __func__);
            break;
        case BZ_UNEXPECTED_EOF:
            fprintf(stderr, "[ERROR][%s] descompression error: the compressed data ends unexpectedly! (traceback: %s)\n", date_str(buf), __func__);
            break;
        default:
            fprintf(stderr, "[ERROR][%s] descompression error: undocumented error! (traceback: %s)\n", date_str(buf), __func__);
    }

    return nullptr;
}

/// @brief Establish a connection and communicate with Aristarchos
/// This function will create a client socket and binf it to Aristarchos. Then 
/// it will send to Aristarchos the command provided in the request buffer. If 
/// needed, it will tray to get back Aristarchos's reply and store it in the
/// reply buffer.
/// @param[in] delay_sec Wait/stall this number of seconds after sending the 
///            request and before accepting the replay
/// @param[in] request A buffer holding the command/request to send to 
///            Aristarchos
/// @param[in] need_reply Set to TRUE if we must expect a reply from Aristarchos
///            (following the sending of the command/request). If set to FALSE, 
///            we are only sending the request and then returning;
/// @param[out] reply If we are expecting an answer from Aristarchos (aka 
///             need_reply is TRUE), this buffer will hold the reply. It must be
///             of size ARISTARCHOS_BUFFER_SIZE
/// @return Anything other than 0 denotes an error
int send_aristarchos_request(int delay_sec, const char* request, int need_reply, char *reply) noexcept {
    char buf[32]; /* for datetime string */
    int error=0;

    try {
        /* create and connect .... */
        printf("[DEBUG][%s] Trying to connect to Aristarchos ...\n", date_str(buf);
        ClientSocket client_socket(ARISTARCHOS_IP, ARISTARCHOS_PORT);
        printf("[DEBUG][%s] Connection with Aristarchos established!\n", date_str(buf));

        /* send request */
        error = client_socket.send(request);
        if (error<0) {
            fprintf(stderr, "[ERROR][%s] Failed sending command to Aristarchos (traceback: %s)\n", date_str(buf), __func__);
        }

        /* if we need to, get the reply */
        if (need_reply) {
            std::memset(reply, '\0', ARISTARCHOS_BUFFER_SIZE);
            /* need this delay for the telescope. Could probably be shorter */
            std::this_thread::sleep_for(std::chrono::seconds{delay_sec});
            error = client_socket.recv(reply, ARISTARCHOS_BUFFER_SIZE);
            if (error<0) {
                fprintf(stderr, "[ERROR][%s] Failed receiving command from Aristarchos (traceback: %s)\n", date_str(buf), __func__);
            }
        }

    } catch (std::exception &e) {
        fprintf(stderr, "[ERROR][%s] Exception caught while connecting to Aristarchos (traceback: %s)\n", date_str(buf), __func__);
        fprintf(stderr, "[ERROR][%s] Cannot connect to Aristarchos! (traceback: %s)\n", buf, __func__);
        error = 1;
    }

    return error;
}

/// @brief Translate a request to an ARISTARCHOS command
/// The translation is performed in the following way:
/// * request: grabHeader -> command: 0003RD;
/// * request callExpStart -> command: 0006RE ON;
/// * request callExpStop -> command: 0006RE OF;
/// * request: getHeaderStatus -> command: 0003RS;
/// @param[in] request A string defining the request to be translated
/// @param[out] command The translated string if the reuest is valid
/// @return A pointer to the command string if the translation was successeful;
///         else, a pointer to null
char *generate_request_string(const char* request, char *command) noexcept {

    char buf[32]; /* for datetime string */
    int status = 0;
    std::memset(command, '\0', ARISTARCHOS_COMMAND_MAX_CHARS);
    
    if (!std::strcmp(request, "grabHeader")) {
        std::strcpy(command, "0003RD;");
    } else if (!std::strcmp(request, "callExpStart")) {
        std::strcpy(command, "0006RE ON;");
    } else if (!std::strcmp(request, "callExpStop")) {
        std::strcpy(command, "0006RE OF;");
    } else if (!std::strcmp(request, "getHeaderStatus")) {
        std::strcpy(command, "0003RS;");
    } else {
        status = 1;
    }

    if (!status) {
        printf("[DEBUG][%s] Command to send to ARISTARCHOS: \"%s\" (translated from: \"%s\")\n", date_str(buf), command, request);
        return command;
    }

    fprintf(stderr, "[ERROR][%s] Failed to translate request: \"%s\" to an ARISTARCHOS command! (traceback: %s)\n", date_str(buf), request, __func__);
    return nullptr;
}

int