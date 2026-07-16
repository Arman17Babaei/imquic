#include "imquic/imquic.h"

#include <stdio.h>

int main(void)
{
	imquic_server *server;
	int ret = imquic_init(NULL);

	if(ret != 0)
		return ret;

	server = imquic_create_server("invalid-prague",
		IMQUIC_CONFIG_INIT,
		IMQUIC_CONFIG_LOCAL_PORT, 0,
		IMQUIC_CONFIG_TLS_CERT, "../.deps/picoquic-l4s/certs/cert.pem",
		IMQUIC_CONFIG_TLS_KEY, "../.deps/picoquic-l4s/certs/key.pem",
		IMQUIC_CONFIG_ALPN, "imquic-l4s-test",
		IMQUIC_CONFIG_CONGESTION_CONTROL, IMQUIC_CONGESTION_PRAGUE,
		IMQUIC_CONFIG_CONGESTION_OPTIONS, "alpha_gain=0/1",
		IMQUIC_CONFIG_DONE, NULL);
	if(server != NULL) {
		fprintf(stderr, "invalid Prague options were accepted\n");
		ret = -1;
	}

	if(ret == 0) {
		server = imquic_create_server("default-prague",
			IMQUIC_CONFIG_INIT,
			IMQUIC_CONFIG_LOCAL_PORT, 0,
			IMQUIC_CONFIG_TLS_CERT, "../.deps/picoquic-l4s/certs/cert.pem",
			IMQUIC_CONFIG_TLS_KEY, "../.deps/picoquic-l4s/certs/key.pem",
			IMQUIC_CONFIG_ALPN, "imquic-l4s-test",
			IMQUIC_CONFIG_CONGESTION_CONTROL, IMQUIC_CONGESTION_PRAGUE,
			IMQUIC_CONFIG_CONGESTION_OPTIONS,
				"alpha_gain=1/16,ce_response=1/2,loss_beta=1/2,sudden_ce_threshold=1/2",
			IMQUIC_CONFIG_DONE, NULL);
		if(server == NULL) {
			fprintf(stderr, "valid Prague endpoint creation failed\n");
			ret = -1;
		}
		else {
			imquic_shutdown_endpoint(server);
		}
	}

	imquic_deinit();
	return ret;
}
