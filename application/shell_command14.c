                    case HC_CMD_DEFRAGMENT:
                        //picofs_consolidate_all_files();
                        picofs_consolidate_all_files_in_flash();
                        break;


                        case HC_CMD_STAT:                    
                        picofs_stat(web.stat_filename);
                        break;                        
                    case HC_CMD_DELETE_FILE:                    
                        hc_delete_file();
                        break;
                    case HC_CMD_DOWNLOAD_FILE:                    
                        hc_download_file();
                        break;      
                    case HC_CMD_VERIFY_FILE:                    
                        hc_verify_file();
                        break;  
                    case HC_CMD_BOOT:
                        hc_boot_file();
                        break;