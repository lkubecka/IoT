#ifndef __HTTPS_H__
#define __HTTPS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "www.howsmyssl.com"
#define WEB_PORT "443"
#define WEB_URL "https://www.howsmyssl.com/a/check"


/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null
   openssl s_client -showcerts -connect io.adafruit.com:443 </dev/null
   openssl s_client -showcerts -connect ridestracker.azurewebsites.net:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");
 
void https_request_task(void *pvParameters);
// static void https_get_task(void *pvParameters) {
//     https_handle_request();
// }

#ifdef __cplusplus
}
#endif

#endif /* __HTTPS_H__ */