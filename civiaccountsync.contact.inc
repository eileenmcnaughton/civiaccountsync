<?php
// $Id$


/*
 *   get contacts from Xero & store / update them in civixero_contacts
 *   use 'civixero_lastxerocontactget' to control date range
 *   No analysis done in this function - just populate the table for matching
 *
 */

function civiaccountsync_contact_pull($xero){
  $last_run = variable_get('civixero_lastxerocontactget', 0);
  if ($last_run > strtotime('2 days ago')) {
    //we will get a min of 2 days to be sure ...
    $last_run = strtotime('2 days ago');
  }
  $params = array(
    'start_date' => date("Y-m-d H:m:s", $last_run),
  );
  try {
    $result = civicrm_api3('xerosync', 'contactpull', $params);
  }
  catch (Exception $e) {
    drupal_set_message($e->getMessage());
  }
  //set now as last synced time
  variable_set('civixero_lastxerocontactget', strtotime('now'));
}

/*
 * Get all contact details for 'active' (generally pending) invoices where the
 * contact has changed since last update. Push to Xero
 */
function civiaccountsync_contact_push($accountsObject){
  $plugin = 'xero';
  try {
    $result = civicrm_api3('xerosync', 'contactpush', array());
    $result = _getCiviCRMContactstoUpdate();
  }
  catch (CiviCRM_API3_Exception $e) {
    dpm($e->getMessage());

  }
  variable_set('civixero_lastcivicontactput', strtotime('now'));
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
    $params['options'] = array('limit' => 50, 'sort' => 'id DESC');
 //   $params['sort'] = 'civicrm_log.modified_date ASC';
    //TODO - currently code assumes that 1000 contributions is enough to bring it up-do-date & set last sync to current date
    $params['rowCount'] = 50;

    $result = civicrm_api3('Contribution','Get',$params);
    $contactIDS = array();
    foreach ($result['values'] as $contribution) {
      if(!empty($contactIDS[$contribution['contact_id']])) {
        // lets save ourselves a few look-ups
        continue;
      }
      $contactIDS[$contribution['contact_id']] = 1;
      $accountContact = array('contact_id' => $contribution['contact_id'], 'accounts_needs_update' => 1);
      try {
        $accountContact['id'] = civicrm_api3('account_contact', 'getvalue', array('plugin' => 'xero', 'return' => 'id', 'contact_id' => $contribution['contact_id']));
      }
      catch (CiviCRM_API3_Exception $e) {
        drupal_set_message($e->getMessage());
        // new contact
      }
      $accountContact['plugin'] = 'xero';
      civicrm_api3('account_contact', 'create', $accountContact);
    }
    return $result;
}

