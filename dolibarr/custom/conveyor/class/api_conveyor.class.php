<?php
use Luracast\Restler\RestException;
require_once DOL_DOCUMENT_ROOT.'/api/class/api.class.php';
class Conveyor extends DolibarrApi {
 public function __construct(){global $db; $this->db=$db;}
 private function authorize(){
  if(!DolibarrApiAccess::$user->hasRight('conveyor','operate'))throw new RestException(403);
 }
 /**
  * @url GET /capacity/{warehouse}
  */
 public function capacity($warehouse){
  global $conf;
  $this->authorize();$wh=(int)$warehouse;
  $limit=getDolGlobalInt('CONVEYOR_CAPACITY_'.$wh);
  if($limit<=0)throw new RestException(409,'Capacity not configured');
  $sql='SELECT COALESCE(SUM(s.reel),0) AS used_qty FROM '.MAIN_DB_PREFIX.'entrepot w LEFT JOIN '.MAIN_DB_PREFIX.'product_stock s ON s.fk_entrepot=w.rowid WHERE w.rowid='.$wh.' AND w.entity='.(int)$conf->entity.' GROUP BY w.rowid';
  $res=$this->db->query($sql);if(!$res)throw new RestException(500);
  $row=$this->db->fetch_object($res);if(!$row)throw new RestException(404);
  return array('available'=>((float)$row->used_qty+1 <= $limit));
 }
 /**
  * @url POST /incidents
  */
 public function incident($movementcode,$expected_exit,$observed_exit){
  global $conf;
  $this->authorize();
  if(!preg_match('/^[a-f0-9]{32}$/D',$movementcode)||!in_array($expected_exit,array(1,2,3))||!in_array($observed_exit,array(-1,0,1,2,3)))throw new RestException(400);
  $table=MAIN_DB_PREFIX.'conveyor_incident';$entity=(int)$conf->entity;
  $sql="INSERT INTO ".$table." (entity,movementcode,expected_exit,observed_exit) VALUES (".$entity.",'".$movementcode."',".(int)$expected_exit.",".(int)$observed_exit.")";
  if($this->db->query($sql))return (int)$this->db->last_insert_id($table);
  $res=$this->db->query("SELECT rowid,expected_exit,observed_exit FROM ".$table." WHERE entity=".$entity." AND movementcode='".$movementcode."'");
  if(!$res||!($r=$this->db->fetch_object($res)))throw new RestException(500);
  if($r->expected_exit!=$expected_exit||$r->observed_exit!=$observed_exit)throw new RestException(409);
  return (int)$r->rowid;
 }
}
