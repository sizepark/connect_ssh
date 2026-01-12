#include <stdio.h>
#include <stdlib.h>
#include <libssh/libssh.h>
#include <unistd.h>

#define TARGET_USER "bandit0"
#define TARGET_PASS "bandit0"
#define TARGET_HOST "bandit.labs.overthewire.org"
#define TARGET_PORT 2220

int main(){
	ssh_session m_session = NULL;
	ssh_channel m_channel = NULL;
	int rc; // return code
	int port = TARGET_PORT;
	int exit_code = 0;

	m_session = ssh_new();
	if (m_session == NULL) {
		fprintf(stderr, "Failed to create SSH session.\n");
		return -1;
	}

	ssh_options_set(m_session, SSH_OPTIONS_HOST, TARGET_HOST);
	ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
	ssh_options_set(m_session, SSH_OPTIONS_USER, TARGET_USER);
	
	printf("[+] Connectin to %s on port %d...\n", TARGET_HOST, port);
	rc = ssh_connect(m_session);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to connect: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}

	rc = ssh_userauth_password(m_session, NULL, TARGET_PASS);
	if (rc != SSH_AUTH_SUCCESS) {
		fprintf(stderr, "Failed to authenticate: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}

	m_channel = ssh_channel_new(m_session);
	if (m_channel == NULL) {
		fprintf(stderr, "Failed to create channel.\n");
		exit_code = -1;
		goto cleanup;
	}

	rc = ssh_channel_open_session(m_channel);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to open channel session: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}
	
	char buffer[1024];
	int nbytes;
	rc = ssh_channel_request_exec(m_channel, "ls -al");
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to execute command: %s\n", ssh_get_error(m_session));
		exit_code = -1;
		goto cleanup;
	}

	// 채널이 닫힐 때까지 출력을 읽어옵니다.
	while ((nbytes = ssh_channel_read(m_channel, buffer, sizeof(buffer), 0)) > 0) {
        if (write(STDOUT_FILENO, buffer, nbytes) != (unsigned int)nbytes) {
			fprintf(stderr, "Failed to write to stdout.\n");
			exit_code = -1;
			goto cleanup;
		}
    }

cleanup:
	if (m_channel) {
		ssh_channel_send_eof(m_channel);
		ssh_channel_free(m_channel); // 이 함수는 채널을 닫는 동작도 포함합니다.
	}
	if (m_session) {
		ssh_disconnect(m_session);
		ssh_free(m_session);
	}
	return exit_code;
}
