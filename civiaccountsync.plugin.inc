<?php

/**
 * CiviAccountSync Plugin class
 * Provides a unified interface for invoking plugins and their jobs
 *
 * AndyW - Circle Interactive 2013
 */

class CiviAccountSyncPlugin {

    private $plugin_name, $initialized;

    public function __construct($plugin_name) {
        
        // Attempt to load civiaccountsync.inc from plugin module's directory - but do not 
        // enforce, so as these functions can also live in the module's .module file if desired.
        module_load_include('inc', $plugin_name, 'civiaccountsync');
        $this->plugin_name = $plugin_name;

        // Attempt to run plugin's init function if one exists
        if (function_exists($plugin_name . '_init')) {
            try {
                call_user_func($plugin_name . '_init');
                $this->initialized = true;
            } catch (Exception $e) {
                civiaccountsync_error(t('CiviAccountSync was unable to initialize the @plugin plugin because @excuse', 
                    array(
                        '@excuse' => $e->getMessage(),
                        '@plugin' => $plugin_name
                    )
                ));
                $this->initialized = false;                  
            }
        }

    }

    public function __call($capability, $arguments) {

        $capability    = $this->plugin_name . '_' . str_replace('-', '_', $capability);
        $last_run_info = variable_get('civiaccountsync_last_run_info', array());

        // check handler exists
        if (!function_exists($capability)) {
            civiaccountsync_error(t("Capability '@capability' not implemented", array('@capability' => $capability)));
            return false;
        }

        // if so, try to run it
        try {
            $result = $capability(isset($last_run_info[$this->plugin_name][$capability]) ? $last_run_info[$this->plugin_name][$capability] : 0);
        } catch (Exception $e) {
            civiaccountsync_error(t('CiviAccountSync encountered an error while running sync job in @file at @line: @error', 
                array(
                    '@error' => $e->getMessage(),
                    '@file'  => $e->getFile(),
                    '@line'  => $e->getLine()
                )
            ));
            return false;
        }

        $last_run_info[$plugin_name][$capability] = time();
        variable_set('civiaccountsync_last_run_info', $last_run_info);
        return;
    
    }

    public function capabilities($plugin = null) {
        
        // optional plugin arg allows us to call this method statically, without full
        // class initialization (preferable if we only want to get a list of capabilities)
        if ($plugin) {
            module_load_include('inc', $plugin, 'civiaccountsync');
            return call_user_func($plugin . '_capabilities');
        }

        return call_user_func($this->plugin_name . '_capabilities');
    
    }

    public function is_initialized() {
        return $this->initialized;
    }

};