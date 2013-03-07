-- Copyright (C) 2011-2013 BlizzLikeCore <http://blizzlike.servegame.com/>
-- ---------------------------------------------------
-- BlizzLike Script to Remove MySQL User and Tables --
-- ---------------------------------------------------

REVOKE ALL PRIVILEGES ON * . * FROM 'blizzlike'@'localhost';

REVOKE ALL PRIVILEGES ON `world` . * FROM 'blizzlike'@'localhost';
REVOKE GRANT OPTION ON `world` . * FROM 'blizzlike'@'localhost';

REVOKE ALL PRIVILEGES ON `characters` . * FROM 'blizzlike'@'localhost';
REVOKE GRANT OPTION ON `characters` . * FROM 'blizzlike'@'localhost';

REVOKE ALL PRIVILEGES ON `auth` . * FROM 'blizzlike'@'localhost';
REVOKE GRANT OPTION ON `auth` . * FROM 'blizzlike'@'localhost';

DROP USER 'blizzlike'@'localhost';

DROP DATABASE IF EXISTS `world` ;
DROP DATABASE IF EXISTS `characters` ;
DROP DATABASE IF EXISTS `auth` ;