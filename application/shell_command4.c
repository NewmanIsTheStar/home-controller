                    case HC_CMD_LIGHTS:
                        shelly_http_request(HTTP_GET, "/relay/1?turn=on", "192.168.33.165", NULL);
                        //config_mmap_test();
                        //config_write_to_file("config.bin"); 
                        // config_mmap("config.bin");
                        // strcpy(cfg->automation_name[31], "A31 set via mmap");
                        // picofs_msync(cfg, sizeof(APP_CONFIG_T), MS_SYNC);
                        break; 
