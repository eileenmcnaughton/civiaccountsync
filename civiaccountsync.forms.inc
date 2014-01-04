<?php
// $Id$


function civiaccountsync_form_contact(&$form_state,$cas_cid = 0){

  if(empty($cas_cid)){
    $cas_cid = civiaccountsync_get_nextUnmatchedContact();
  }

  //todo - handlin for no argument
  $sql = "SELECT acc_full_name, acc_primary_email, acc_first_name, acc_last_name, civi_suggested_contact_id, civi_ignore FROM {cas_contacts} WHERE cas_cid = %d";
  $result = db_query($sql,$cas_cid);

  $data = db_fetch_object($result);

  $form['name']= array(
  '#type' => 'item',
  '#description' => 'Name in Accounts: ' . $data->acc_full_name . " " . $data->acc_primary_email,
  );
  $form['civi_ignore']= array(
  '#title' => t('Exclude from Civi'),
  '#type' => 'checkbox',
  '#description' => 'I don\'t want to put this contact in CiviCRM',
  '#options' => array('0' => 'No', 1 => 'Yes'),
  '#default_value' => $data->civi_ignore,
  );
  //TODO create a fieldset
   $form['civi_create']= array(
  '#title' => t('Create contact in Civi'),
  '#type' => 'checkbox',
  '#description' => 'Create this contact in CiviCRM',
  '#options' => array('0' => 'No', 1 => 'Yes'),
  );
   $form['do_not_email']= array(
  '#title' => t('Set do not e-mail'),
  '#type' => 'checkbox',
  '#description' => 'Create option only Set contact to not receive e-mails from Civi',
  '#options' => array('0' => 'No', 1 => 'Yes'),
  );
   $form['is_opt_out']= array(
  '#title' => t('Set opt out as if user had opted out of CiviMails'),
  '#type' => 'checkbox',
  '#description' => 'Create option only contact to not receive civimails from Civi',
  '#options' => array('0' => 'No', 1 => 'Yes'),
  );
   $form['contact_type']= array(
  '#title' => t('Set opt out as if user had opted out of CiviMails'),
  '#type' => 'select',
  '#description' => 'Create option only contact to not receive civimails from Civi',
  '#options' => array('Individual' => 'Individual', 'Organization' => 'Organization'),
  '#default_value' => 'Individual',
  );
  $form['cas_cid']= array(
  '#type' => 'value',
  '#value' => $cas_cid,
  );
  $form['first_name']= array(
  '#type' => 'value',
  '#value' => $data->acc_first_name,
  );
  $form['last_name']= array(
  '#type' => 'value',
  '#value' => $data->acc_last_name,
  );
  $form['email']= array(
  '#type' => 'value',
  '#value' => $data->acc_primary_email,
  );
   $form['display_name']= array(
  '#type' => 'value',
  '#value' => $data->acc_full_name,
  );
  $form['civicrm_cid']= array(
  '#title' => 'CiviCRM ID',
  '#type' => 'textfield',
  '#description' => t('Specify Civi ID (provide a number)'),
  '#default_value' => $data->civi_suggested_contact_id,
  );
  $form['submit']= array(
  '#type' => 'submit',
  '#value' => 'Submit',
  );

  return $form;
}


function civiaccountsync_form_contact_submit($form,&$form_state){
  if ($form_state['values']['civi_create']){
    if (!module_exists('civicrm')) {
      exit;
   }

    civicrm_initialize(TRUE);
    require_once "api/v2/Contact.php";
    $params['first_name']    = $form_state['values']['first_name'];
    $params['last_name']     = $form_state['values']['last_name'];
    $params['display_name']  = $form_state['values']['display_name'];
    $params['email']         = $form_state['values']['email'];
    $params['do_not_email']  = $form_state['values']['do_not_email'];
    $params['is_opt_out']  = $form_state['values']['is_opt_out'];
    $params['contact_type'] = $form_state['values']['contact_type'];
    if ($params['contact_type'] == 'Organization'){
      $params['organization_name'] = $params['display_name'];
        }

    $params['version'] = 3;
    $result = civicrm_contact_add($params);
  }
   drupal_set_message("<pre>".$params['display_name'] . " created ". print_r($result,true)."</pre>");
    $table = 'cas_contacts';
    $record = new stdClass();
    $record->civi_ignore = $form_state['values']['civi_ignore'];

    $record->cas_cid = $form_state['values']['cas_cid'];
    if ($result['contact_id']){
      $record->civicrm_cid =$result['contact_id'];
    }else{
      $record->civicrm_cid = $form_state['values']['civicrm_cid'];
    }
    $update = 'cas_cid';
    drupal_set_message("hi<pre>". print_r($record,true)."</pre>");

    drupal_write_record($table,$record,$update);


  $nextrecord = civiaccountsync_get_nextUnmatchedContact();
  $form_state['redirect'] = 'cas/contact/' . $nextrecord;
}

/*
 * Form to manage the account sync process
 */
function civiaccountsync_form_dashboard(){
    module_load_include('inc','civiaccountsync','civiaccountsync.contact');
  $form['civiaccountsync_contact_pull']= array(
    '#type' => 'checkbox',
    '#title' => t('Pull Contacts from Accounts Package. Last done on ' . date("Y-m-d H:m:s",variable_get('civixero_lastxerocontactget',0))),
    '#default_value' => 1,
  );
  if(($contactsUpdateCount = _getCiviCRMContactstoUpdate(1))>0){
    $generateContactstoUpdate = ' Required for ' . $contactsUpdateCount ." contacts" . $generateContactstoUpdate;
  }

  $form['civiaccountsync_contact_push']= array(
    '#type' => 'checkbox',
    '#title' => t('Push contacts details to Accounts Package (for contacts with pending invoices  )' ),
    '#default_value' => 1,
  );
  $form['civiaccountsync_invoice_pull']= array(
    '#type' => 'checkbox',
    '#title' => t('Pull invoice Updates From Accounts Package since ' .  date('Y-m-d h-i-s', variable_get ( 'civixero_lastxeroinvoiceget',0))),
    '#default_value' => 1,
  );

  $form['civiaccountsync_invoice_push']= array(
    '#type' => 'checkbox',
    '#title' => t('Push new Invoices from CiviCRM to Xero'),
    '#default_value' => 1,
  );

  $form['civiaccountsync_invoice_complete_paid']= array(
    '#type' => 'checkbox',
    '#title' => t('Complete paid pending contributions based on Accounts Package Payment Information'),
    '#default_value' => 1,
  );


  $form['civiaccountsync_invoice_push_cancel']= array(
    '#type' => 'checkbox',
    '#title' => t('Cancel invoices in Accounts Package based on them being cancelled in CiviCRM'),
    '#default_value' => 1,
  );
  $form['civicaccountsync_invoice_cancel_inCivi']= array(
    '#type' => 'checkbox',
    '#title' => t('Cancel invoices in CiviCRM based on them being cancelled voided or deleted in Accounts Package'),
    '#default_value' => 1,
  );



  $form['submit']= array(
  '#type' => 'submit',
  '#value' => 'Submit',
  );

  return $form;
}

/*
 * implement form submit to process actions
 */
function civiaccountsync_form_dashboard_submit($form,&$form_state){

  module_load_include('inc','civiaccountsync','civiaccountsync.queries');
  $accountsObj = civiaccountsync_init();
  foreach ($form_state['values'] as $value => $key) {

   if($key ==1){
     $value($accountsObj);
   }
  }
}
/*
 * Get next accounts contact not matched to a CiviCRM record
 */
function civiaccountsync_get_nextUnmatchedContact(){
  $nextRecord = db_result(db_query("SELECT cas_cid FROM {cas_contacts} WHERE civicrm_cid =0 AND civi_suggested_contact_id IS NULL AND civi_ignore = 0 LIMIT 1"));
  return $nextRecord;

}