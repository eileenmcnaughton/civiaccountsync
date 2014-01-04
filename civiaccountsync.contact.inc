<?php
// $Id$


/*
 *   get contacts from Xero & store / update them in civixero_contacts
 *   use 'civixero_lastxerocontactget' to control date range
 *   No analysis done in this function - just populate the table for matching
 *
 */

function civiaccountsync_contact_pull($xero){

  $table = 'cas_contacts';
  $result = $xero->Contacts(false,date("Y-m-d H:m:s",variable_get('civixero_lastxerocontactget',0)));

  if(!is_array($result)){
    echo $result;
    die;
  }
  if ($result['Contacts']){
  drupal_set_message(t(count($result['Contacts'] . "retrieved")));
    foreach($result['Contacts']['Contact'] as $contact){
    $record = new stdClass();

    $cas_cid  = db_result(db_query("SELECT cas_cid FROM {cas_contacts} WHERE acc_cid = '%s'", $contact['ContactID']));
    if (!empty($cas_cid) ){
      $update = 'cas_cid';
      $record->cas_cid = $cas_cid;
    }

      if($contact['IsSupplier'] =='true'){
       $record->acc_is_supplier  = 1;
      }
      if($contact['IsCustomer'] =='true'){
       $record->acc_is_customer  = 1;
      }
     $record->acc_primary_email  = $contact['EmailAddress'];
     $record->acc_full_name =$contact['Name'];
      if ($contact['FirstName'] && $contact['LastName']){
        //we have name break down so use it
       $record->acc_first_name = $contact['FirstName'];
       $record->acc_last_name = $contact['LastName'];

      }else{
         $name = explode(" ",$contact['Name']);
         $record->acc_first_name = $name[0];
         $record->acc_last_name = $name[1];
         if ($name[2]){
           $record->acc_last_name = $name[2];
         }

      }

       $record->civicrm_cid = $contact['ContactNumber'];
       $record->acc_cid = $contact['ContactID'];
       $record->last_cas_mod = strtotime($contact['UpdatedDateUTC']);
       drupal_write_record($table,$record,$update);
       unset($update);
  }
  }
  //set now as last synced time
  variable_set('civixero_lastxerocontactget',strtotime('now'));
}

/*
 * Get all contact details for 'active' (generally pending) invoices where the
 * contact has changed since last update. Push to Xero
 */
function civiaccountsync_contact_push($accountsObject){

  $result = _getCiviCRMContactstoUpdate();
  foreach($result['values'] as $contribution){
    $accountsContactID = db_result(db_query("SELECT  acc_cid FROM {cas_contacts} WHERE civicrm_cid = '%d'",$contribution['contact_id']));
    $civiCRMcontact  = _getContactDetailsFromCivi($contribution['contact_id']);
    $accountsContact = _civiaccountsync_contact_mapCiviCRMParamstoAccountsParams($civiCRMcontact, $accountsContactID);

    $accountsObject->Contacts( $accountsContact );
  }
  variable_set('civixero_lastcivicontactput',strtotime('now'));
}

/*
 * Function looks at all contacts with a Xero ID & no CiviCRM ID &
 * populates table with possible CiviCRM contact IDs  For approval
 */
function civiaccountsync_generate_contactCivicrmSuggestions(){

  $sql = "SELECT cas_cid,  acc_first_name as first_name, acc_last_name as last_name FROM {cas_contacts} WHERE civicrm_cid =0  AND (civi_suggested_contact_id IS NULL OR civi_suggested_contact_id =0 )AND civi_ignore = 0";
  $contacts = db_query($sql);
  $table = 'cas_contacts';
    while ($params = db_fetch_array($contacts)) { // this returns the row as an array, use db_fetch_object to get an object
    try{
      $result = civicrm_api3('contact', 'get', $params);
       list( $params['civi_suggested_contact_id']) = array_keys($result['values']);
       if (variable_get('civixero_automaticallymatchcontact',0) ==1 ){
         $params['civicrm_cid'] = $params['civi_suggested_contact_id'];
       }
        $record = (object)$params;
        $update = 'cas_cid';
        drupal_write_record($table,$record,$update);
        drupal_set_message(t("generated suggestion for " ) . $params['acc_first_name']);
    }
    catch(Exception $e) {
    }

  }

}
/*
 * Get contact details for any contacts with 'current' (pending) contributions that have
 * changed since the last update
 * (note the contact object rather is the one whose modification date is tracked
 *
 * return array $result array of contributions in pending status where contact modified since last update
 */
function _getCiviCRMContactstoUpdate($count = 0){
    if ( ! civicrm_initialize( ) ) {
        return;
    }

    $lastUpdate = date("Y-m-d",(int) variable_get('civixero_lastcivicontactput','0'));
    $params['contribution_status_id'] =2;
    $params['modified_date_low']= $lastUpdate;
 //   $params['sort'] = 'civicrm_log.modified_date ASC';
    //TODO - currently code assumes that 1000 contributions is enough to bring it up-do-date & set last sync to current date
    $params['rowCount'] = 50;
    if (!empty($count)){
      return $result['count'];
    }
    $params['version'] =3;
    $result = civicrm_api('Contribution','Get',$params);
    return $result;
}

/*
 * Get contact Details out of CiviCRM
 */

function _getContactDetailsFromCivi($contactID){

  $params = array('id'  => $contactID,
                   'version'  => 3,);
  $contact = civicrm_api('Contact', 'Get', $params);
  return($contact['values'][$contact['id']]);

}

/*
 * Map civicrm Array to Accounts package field names
 * @param array $contact Contact Array as returned from API
 * @param string	accountsID ID from Accounting system
 * @return $accountsContact Contact Object/ array as expected by accounts package
 */
function _civiaccountsync_contact_mapCiviCRMParamstoAccountsParams($contact, $accountsID){
  $new_contact = array(
    array(
        "Name" => $contact['display_name'],
        "FirstName" => $contact['first_name'],
        "LastName" => $contact['last_name'],
        "EmailAddress" => $contact['email'],
        "ContactNumber" => $contact['contact_id'],
        "Addresses" => array(
            "Address" => array(
                array(
                    "AddressType" => 'POBOX',//described in documentation as the default mailing address for invoices http://blog.xero.com/developer/api/types/#Addresses
                    "AddressLine1" => $contact['street_address'],
                    "City" => $contact['city'],
                    "PostalCode" => $contact['postal_code'],
                ),

            )
        ),
        "Phones"  => array(
        			"Phone" => array("PhoneType" => 'DEFAULT',
                             "PhoneNumber" => $contact['phone'],
                            ),
        )
    )
);
  if (!empty($accountsID)){
    $new_contact['contactID'] = $accountsID;
  }

return $new_contact ;
}


/*
 * Calculate how many contacts are waiting for suggestions to be generated
 */

function civicaccountsync_getUnmatchedContactCount(){

    $sql = "SELECT count(cas_cid) FROM {cas_contacts} WHERE civicrm_cid =0  AND (civi_suggested_contact_id IS NULL OR civi_suggested_contact_id =0 )AND civi_ignore = 0";

}