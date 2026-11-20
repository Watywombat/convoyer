CREATE TABLE llx_conveyor_incident (
 rowid integer AUTO_INCREMENT PRIMARY KEY,
 entity integer NOT NULL,
 movementcode varchar(32) NOT NULL,
 expected_exit integer NOT NULL,
 observed_exit integer NOT NULL,
 created_at timestamp DEFAULT CURRENT_TIMESTAMP,
 UNIQUE (entity,movementcode)
) ENGINE=innodb;
