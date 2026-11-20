<?php
require_once DOL_DOCUMENT_ROOT.'/core/modules/DolibarrModules.class.php';
class modConveyor extends DolibarrModules {
 public function __construct($db) {
  global $conf;
  $this->db=$db; $this->numero=501930; $this->rights_class='conveyor';
  $this->family='products'; $this->name='Conveyor'; $this->description='Capacité et incidents du convoyeur';
  $this->version='1.1.0'; $this->const_name='MAIN_MODULE_CONVEYOR';
  $this->rights=array(); $this->rights[0]=array(501931,'Consulter la capacité et signaler un incident',0,0,'operate');
  $this->depends=array('modStock','modApi');
 }
 public function init($options='') {
  if($this->_load_tables('/conveyor/sql/')<0)return -1;
  return $this->_init(array(),$options);
 }
 public function remove($options='') {return $this->_remove(array(),$options);}
}
