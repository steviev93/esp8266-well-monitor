idf_component_register(SRCS "main.c" 
                            "../hal/i2c_master.c" 
                            "../drivers/ads1115.c" 
                            "../services/wifi_manager.c" 
                            "../application/telemetry.c" 
                            "../application/buffer.c" 
                            "../application/supervisor.c" 
                            "../services/nvs_service.c"
INCLUDE_DIRS ".")