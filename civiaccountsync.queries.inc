<?php
// $Id$

/**
 * Implementation of hook_init().
 */
function civiaccountsync_init(){
//todo - this just calls civixero - make it more generic
//todo dumb name for file
  // Automatically include the action files.
  module_load_include('inc','civiaccountsync','civiaccountsync.contact');
  module_load_include('inc','civiaccountsync','civiaccountsync.invoice');
  $files = file_scan_directory(drupal_get_path('module', 'civixero'), '(.*).civiaccountsync.inc$');

  if ($files) foreach ($files as $file) {
    require_once($file->filename);
    $pos = strpos($file->basename, ".");
    $plugin = substr($file->basename,0,$pos);
    $functionName = $plugin . "_civiaccountsync_initialise";
    $accountsObject = $functionName();
    if (!empty($accountsObject)){
      return $accountsObject;
    }
  }
}
