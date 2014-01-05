<?php
// $Id$


/*
 *   get contacts from Xero & store / update them in civixero_contacts
 *   use 'civixero_lastxerocontactget' to control date range
 *   No analysis done in this function - just populate the table for matching
 *
 */

function civiaccountsync_contact_pull($xero){
  $plugin = 'xero';
  $result = $xero->Contacts(false, date("Y-m-d H:m:s", variable_get('civixero_lastxerocontactget',0)));
  if(!is_array($result)){
    echo $result;
    die;
  }
  if ($result['Contacts']){
    CRM_Core_Session::setStatus(count($result['Contacts'] . ts(' retrieved')), ts('Contact Pull'));
    foreach($result['Contacts']['Contact'] as $contact){
      $params = array();
      try {
        $params['id'] = civicrm_api3('account_contact', 'getvalue', array(
          'return' => 'id',
          'accounts_contact_id' => $contact['ContactID'],
          'plugin' => $plugin,
        ));
      }
      catch (CiviCRM_API3_Exception $e) {
        // this is an update
      }
      $params['accounts_display_name'] = $contact['Name'];
      $params['contact_id'] = $contact['ContactNumber'];
      $params['accounts_modified_date'] = $contact['UpdatedDateUTC'];
      $params['plugin'] = 'xero';
      $params['accounts_contact_id'] = $contact['ContactID'];
      $params['accounts_data'] = json_encode($contact);
      try {
        $result = civicrm_api3('account_contact', 'create', $params);
      }
      catch (CiviCRM_API3_Exception $e) {
        CRM_Core_Session::setStatus(ts('Failed to store ') . $params['accounts_display_name']
        . ts(' with error ') . $e->getMessage()
        , ts('Contact Pull failed'));
      }
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
  $plugin = 'xero';
  $result = _getCiviCRMContactstoUpdate();

  foreach($result['values'] as $contribution){
    try {
    $accountsContactID = civicrm_api3('account_contact', 'getvalue', array(
      'return' => 'accounts_contact_id',
      'contact_id' => $contribution['contact_id'],
      'plugin' => $plugin,
    ));
    }
    catch (CiviCRM_API3_Exception $e) {
      //we are creating a new contact - but we should add an option to update only or so review new contacts
    }
    $civiCRMcontact  = _getContactDetailsFromCivi($contribution['contact_id']);
    $accountsContact = _civiaccountsync_contact_mapCiviCRMParamstoAccountsParams($civiCRMcontact, $accountsContactID);

    $accountsObject->Contacts( $accountsContact );
  }
  variable_set('civixero_lastcivicontactput',strtotime('now'));
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
 * @param string  accountsID ID from Accounting system
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
