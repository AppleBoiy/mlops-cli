#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/wait.h>

/*
 * Raw POSIX Socket HTTP Fetcher
 * Makes a zero-dependency HTTP/1.0 GET request to the GCP Metadata Server.
 * Executes in < 1ms on GCP VMs.
 */
static int fetch_gcp_metadata(const char *path, char *out_body, size_t max_len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    server.sin_addr.s_addr = inet_addr("169.254.169.254");
    
    /* Set a strict 1-second timeout in case we are running outside of GCP */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(sock);
        return -1;
    }
    
    char req[512];
    snprintf(req, sizeof(req), 
             "GET /computeMetadata/v1/%s HTTP/1.0\r\n"
             "Host: metadata.google.internal\r\n"
             "Metadata-Flavor: Google\r\n\r\n", path);
             
    if (send(sock, req, strlen(req), 0) < 0) {
        close(sock);
        return -1;
    }
    
    char resp[4096] = {0};
    int total = 0, bytes;
    while ((bytes = recv(sock, resp + total, sizeof(resp) - total - 1, 0)) > 0) {
        total += bytes;
    }
    close(sock);
    
    if (total == 0) return -1;
    
    /* Parse out the HTTP body (skip headers) */
    char *body_ptr = strstr(resp, "\r\n\r\n");
    if (!body_ptr) {
        body_ptr = strstr(resp, "\n\n");
        if (!body_ptr) return -1;
        body_ptr += 2;
    } else {
        body_ptr += 4;
    }
    
    /* If HTTP 200 is not in the first line, treat as failure (e.g. 404) */
    if (strncmp(resp, "HTTP/1.0 200", 12) != 0 && strncmp(resp, "HTTP/1.1 200", 12) != 0) {
        return -1;
    }
    
    strncpy(out_body, body_ptr, max_len - 1);
    out_body[max_len - 1] = '\0';
    return 0;
}

/*
 * Identity & Metadata Debugger
 */
static int cmd_gcp_whoami(int argc, char **argv) {
    int json = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) json = 1;
    }
    
    char email[256] = {0};
    char project[256] = {0};
    char zone_full[256] = {0};
    
    if (fetch_gcp_metadata("instance/service-accounts/default/email", email, sizeof(email)) != 0) {
        if (json) {
            printf("{\"error\":\"Could not reach GCP Metadata Server\"}\n");
        } else {
            fprintf(stderr, "Error: Could not reach GCP Metadata Server (169.254.169.254).\n");
            fprintf(stderr, "Are you running this directly on a GCP VM?\n");
        }
        return 1;
    }
    
    fetch_gcp_metadata("project/project-id", project, sizeof(project));
    fetch_gcp_metadata("instance/zone", zone_full, sizeof(zone_full));
    
    /* Zone returns as 'projects/12345/zones/us-central1-a', so we extract the suffix */
    char *zone = strrchr(zone_full, '/');
    if (zone) zone++; else zone = zone_full;
    
    char tags[1024] = {0};
    int has_tags = (fetch_gcp_metadata("instance/tags", tags, sizeof(tags)) == 0);

    if (json) {
        printf("{\"project_id\":\"%s\",\"zone\":\"%s\",\"service_account\":\"%s\"", project, zone, email);
        if (has_tags) {
            printf(",\"tags\":%s", tags);
        }
        printf("}\n");
    } else {
        printf("GCP Environment Context:\n");
        printf("----------------------------------------------------\n");
        printf("Project ID:      %s\n", project);
        printf("Zone:            %s\n", zone);
        printf("Service Account: %s\n", email);
        if (has_tags) {
            printf("Tags:            %s\n", tags);
        }
    }
    
    return 0;
}

/*
 * Preemptible / Spot Instance Watcher
 */
static int cmd_gcp_spot_watch(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[0], "--exec") != 0) {
        fprintf(stderr, "Usage: mops gcp spot-watch --exec \"<command>\"\n");
        return 1;
    }
    
    const char *cmd = argv[1];
    printf("Monitoring for GCP Spot Preemption. Polling Metadata Server...\n");
    printf("When termination signal is received, executing: %s\n", cmd);
    
    char status[32];
    
    while (1) {
        if (fetch_gcp_metadata("instance/preempted", status, sizeof(status)) == 0) {
            if (strncmp(status, "TRUE", 4) == 0) {
                printf("\n[WARNING] Preemption signal received from GCP! Executing checkpoint command...\n");
                int ret = system(cmd);
                exit(WIFEXITED(ret) ? WEXITSTATUS(ret) : 1);
            }
        }
        /* GCP sends the termination signal 30 seconds before shutdown. Polling every 5s is highly safe. */
        sleep(5);
    }
    
    return 0;
}

/*
 * IAP Tunneling Shortcut
 */
static int cmd_gcp_tunnel(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mops gcp tunnel <instance-name> <remote-port>:<local-port>\n");
        return 1;
    }
    
    const char *instance = argv[0];
    char ports[256];
    strncpy(ports, argv[1], sizeof(ports) - 1);
    
    const char *remote = strtok(ports, ":");
    const char *local = strtok(NULL, ":");
    
    if (!remote || !local) {
        fprintf(stderr, "Error: Ports must be formatted as <remote-port>:<local-port> (e.g., 6006:8080)\n");
        return 1;
    }
    
    char project[256] = {0};
    char zone_full[256] = {0};
    char cmd[1024];
    
    /* If run on a bastion, auto-fetch project and zone, otherwise let gcloud use local defaults */
    if (fetch_gcp_metadata("project/project-id", project, sizeof(project)) == 0 &&
        fetch_gcp_metadata("instance/zone", zone_full, sizeof(zone_full)) == 0) {
        
        char *zone = strrchr(zone_full, '/');
        if (zone) zone++; else zone = zone_full;
        
        snprintf(cmd, sizeof(cmd), 
                 "gcloud compute ssh %s --tunnel-through-iap --project %s --zone %s -- -L %s:localhost:%s", 
                 instance, project, zone, local, remote);
    } else {
        snprintf(cmd, sizeof(cmd), 
                 "gcloud compute ssh %s --tunnel-through-iap -- -L %s:localhost:%s", 
                 instance, local, remote);
    }
    
    printf("Starting GCP IAP tunnel. Forwarding local port %s to remote port %s...\n", local, remote);
    return system(cmd);
}

/*
 * Secure Secret Injection
 */
static int cmd_gcp_run_with_secrets(int argc, char **argv) {
    if (argc < 2 || strncmp(argv[0], "--secret=", 9) != 0) {
        fprintf(stderr, "Usage: mops gcp run-with-secrets --secret=\"<ENV_VAR>=<SECRET_NAME>\" \"<command>\"\n");
        return 1;
    }
    
    char mapping[256];
    strncpy(mapping, argv[0] + 9, sizeof(mapping) - 1);
    
    char *env_var = strtok(mapping, "=");
    const char *secret_name = strtok(NULL, "=");
    
    if (!env_var || !secret_name) {
        fprintf(stderr, "Error: Secret format must be ENV_VAR=SECRET_NAME\n");
        return 1;
    }
    
    const char *cmd = argv[1];
    
    /* Fetch payload securely via gcloud in-memory */
    char gcloud_cmd[512];
    snprintf(gcloud_cmd, sizeof(gcloud_cmd), 
             "gcloud secrets versions access latest --secret=\"%s\" 2>/dev/null", 
             secret_name);
             
    FILE *fp = popen(gcloud_cmd, "r");
    if (!fp) {
        fprintf(stderr, "Error: Failed to execute gcloud process.\n");
        return 1;
    }
    
    char secret_val[4096] = {0};
    size_t bytes = fread(secret_val, 1, sizeof(secret_val) - 1, fp);
    pclose(fp);
    
    if (bytes == 0) {
        fprintf(stderr, "Error: Could not retrieve secret '%s' from GCP Secret Manager.\n", secret_name);
        return 1;
    }
    
    /* Trim trailing newline from standard gcloud outputs */
    if (secret_val[bytes - 1] == '\n') {
        secret_val[bytes - 1] = '\0';
    }
    
    setenv(env_var, secret_val, 1);
    
    /* Zero out the buffer memory immediately */
    memset(secret_val, 0, sizeof(secret_val));
    
    printf("Successfully mapped GCP Secret '%s' into environment variable '%s'.\n", secret_name, env_var);
    printf("Executing command...\n");
    
    int ret = system(cmd);
    return WIFEXITED(ret) ? WEXITSTATUS(ret) : 1;
}

/*
 * Main Dispatcher for `gcp` subcommand
 */
int cmd_gcp(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mops gcp <command> [options]\n");
        fprintf(stderr, "Run 'mops gcp --help' for more information.\n");
        return 1;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "--help") == 0 || strcmp(subcmd, "-h") == 0) {
        printf("Usage: mops gcp <command> [options]\n\n");
        printf("Commands:\n");
        printf("  whoami               Instantly print active Service Account, Project, and Zone (zero-auth)\n");
        printf("  spot-watch           Poll for spot termination and trigger script (--exec \"<cmd>\")\n");
        printf("  tunnel               Establish an IAP tunnel (<instance> <remote-port>:<local-port>)\n");
        printf("  run-with-secrets     Fetch a Secret Manager payload and inject it (--secret=\"ENV=NAME\" \"<cmd>\")\n\n");
        printf("Options:\n");
        printf("  --json               Output in JSON format (for 'whoami' command)\n");
        return 0;
    } else if (strcmp(subcmd, "whoami") == 0) {
        return cmd_gcp_whoami(argc - 2, argv + 2);
    } else if (strcmp(subcmd, "spot-watch") == 0) {
        return cmd_gcp_spot_watch(argc - 2, argv + 2);
    } else if (strcmp(subcmd, "tunnel") == 0) {
        return cmd_gcp_tunnel(argc - 2, argv + 2);
    } else if (strcmp(subcmd, "run-with-secrets") == 0) {
        return cmd_gcp_run_with_secrets(argc - 2, argv + 2);
    } else {
        fprintf(stderr, "Unknown gcp command: %s\n", subcmd);
        fprintf(stderr, "Usage: mops gcp <command> [options]\n");
        fprintf(stderr, "Run 'mops gcp --help' for more information.\n");
        return 1;
    }
}