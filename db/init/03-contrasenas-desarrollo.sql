-- Solo para desarrollo: datos-ejemplo.sql trae hashes de muestra con los que nadie
-- puede entrar. Con esto, todos los usuarios de ejemplo entran con Demo_123.
-- En la PC de caja este archivo no se carga.
UPDATE usuario SET contrasena_hash = '$argon2id$v=19$m=65536,t=2,p=1$OGWaM0hP+2WGqiuPKNB+9w$5kmGHLGW/7wuR5A/2AzeCyy7yILtPqfeyV7BKspmuB0';
