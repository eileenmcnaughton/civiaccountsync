<?php
// $Id$
/*
 * Form to manage the account sync process
 */
function civiaccountsync_form_dashboard(){
  $form['civiaccountsync_contact_pull']= array(
    '#type' => 'checkbox',
    '#title' => t('Pull Contacts from Accounts Package. Last done on ' . date("Y-m-d H:m:s",variable_get('civixero_lastxerocontactget',0))),
    '#default_value' => 1,
  );
  $form['civiaccountsync_contact_push']= array(
    '#type' => 'checkbox',
    '#title' => t('Push contacts details to Accounts Package (for contacts with pending invoices  )' ),
    '#default_value' => 1,
  );
  $form['civiaccountsync_invoice_pull']= array(
    '#type' => 'checkbox',
    '#title' => t('Pull invoice Updates From Accounts Package since ' .  date('Y-m-d h-i-s', variable_get ('civixero_lastxeroinvoiceget',0))),
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
  if(!civicrm_initialize()) {
    return;
  }
  module_load_include('inc','civiaccountsync','civiaccountsync.queries');
  $accountsObj = civiaccountsync_init();
  foreach ($form_state['values'] as $value => $key) {
   if($key ==1){
     $value($accountsObj);
   }
  }
}
