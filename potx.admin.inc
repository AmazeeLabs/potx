<?php

/**
 * @file
 *   Administrative interface for the module.
 */

/**
 * Component selection interface.
 */
function potx_select_component_form() {

  $form = array();

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Initialize UOS strings'),
  );

  return $form;
}

/**
 * Generate translation template or translation file for the requested component.
 */
function potx_select_component_form_submit($form, &$form_state) {
  global $devel_shutdown;

  // Avoid devel.module putting extra output to the end of files exported.
  $devel_shutdown = FALSE;

  // This could take some time.
  @set_time_limit(0);
  module_load_include('inc', 'potx');
  module_load_include('inc', 'potx', 'potx.local');

  // Silence status messages.
  potx_status('set', POTX_STATUS_MESSAGE);

  $presets = array(
    'sites/all/modules/custom/uos' => array(
      'functions' => array('t', 'format_plural', 'uos_t', 'uos_format_plural'),
    ),
    'sites/all/modules/custom/abacus' => array(
      'functions' => array('t', 'format_plural'),
    ),
  );
  foreach ($presets as $component => $preset) {

    $pathinfo = pathinfo($component);
    if (!isset($pathinfo['filename'])) {
      // The filename key is only available in PHP 5.2.0+
      $pathinfo['filename'] = substr($pathinfo['basename'], 0, strrpos($pathinfo['basename'], '.'));
    }

    potx_local_init($component);
    $files = _potx_explore_dir($component . '/');
    $strip_prefix = 1 + strlen($component);

    // Collect every string in affected files. Installer related strings are discared.
    foreach ($files as $file) {
      _potx_process_file($preset, $file, $strip_prefix, '_potx_init_string_translation');
    }
  }
}

/**
 * Build a chunk of the component selection form.
 *
 * @param $form
 *   Form to populate with fields.
 * @param $components
 *   Structured array with components as returned by _potx_component_list().
 * @param $dirname
 *   Name of directory handled.
 */
function _potx_component_selector(&$form, &$components, $dirname = '') {

  // Pop off count of components in this directory.
  if (isset($components['#-count'])) {
    $component_count = $components['#-count'];
    unset($components['#-count']);
  }

  //ksort($components);
  $dirkeys = array_keys($components);

  // A directory with one component.
  if (isset($component_count) && (count($components) == 1)) {
    $component = array_shift($components);
    $dirname = dirname($component->filename);
    $form[_potx_form_id('dir', $dirname)] = array(
      '#type' => 'radio',
      '#title' => t('Extract from %name in the %directory directory', array('%directory' => $dirname, '%name' => $component->name)),
      '#description' => t('Generates output from all files found in this directory.'),
      '#default_value' => 0,
      '#return_value' => $dirname,
      // Get all radio buttons into the same group.
      '#parents' => array('component'),
    );
    return;
  }

  // A directory with multiple components in it.
  if (preg_match('!/(modules|themes)\\b(/.+)?!', $dirname, $pathmatch)) {
    $t_args = array('@directory' => substr($dirname, 1));
    if (isset($pathmatch[2])) {
      $form[_potx_form_id('dir', $dirname)] = array(
        '#type' => 'radio',
        '#title' => t('Extract from all in directory "@directory"', $t_args),
        '#description' => t('To extract from a single component in this directory, choose the desired entry in the fieldset below.'),
        '#default_value' => 0,
        '#return_value' => substr($dirname, 1),
        // Get all radio buttons into the same group.
        '#parents' => array('component'),
      );
    }
    $element = array(
      '#type' => 'fieldset',
      '#title' => t('Directory "@directory"', $t_args),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $form[_potx_form_id('fs', $dirname)] =& $element;
  }
  else {
    $element =& $form;
  }

  foreach ($dirkeys as $entry) {
    // A component in this directory with multiple components.
    if ($entry[0] == '#') {
      // Component entry.
      $t_args = array(
        '%directory' => dirname($components[$entry]->filename),
        '%name'      => $components[$entry]->name,
        '%pattern'   => $components[$entry]->name .'.*',
      );
      $element[_potx_form_id('com', $components[$entry]->basename)] = array(
        '#type' => 'radio',
        '#title' => t('Extract from %name', $t_args),
        '#description' => t('Extract from files named %pattern in the %directory directory.', $t_args),
        '#default_value' => 0,
        '#return_value' => $components[$entry]->filename,
        // Get all radio buttons into the same group.
        '#parents' => array('component'),
      );
    }
    // A subdirectory we need to look into.
    else {
      _potx_component_selector($element, $components[$entry], "$dirname/$entry");
    }
  }

  return count($components);
}

/**
 * Generate a sane form element ID for the current radio button.
 *
 * @param $type
 *   Type of ID generated: 'fs' for fieldset, 'dir' for directory, 'com' for component.
 * @param $path
 *   Path of file we generate an ID for.
 * @return string
 *   The generated ID.
 */
function _potx_form_id($type, $path) {
  return 'potx-'. $type .'-'. preg_replace('/[^a-zA-Z0-9]+/', '-', $path);
}

/**
 * Generate a hierarchical structured list of components.
 *
 * @return array
 *  Array in the directory structure identified.
 *    - 'normal'  keyed elements being subfolders
 *    - '#name'   elements being component objects for the 'name' component
 *    - '#-count' being the file count of all components in the directory
 */
function _potx_component_list() {
  $components = array();
  // Get a list of all enabled modules and themes.
  $result = db_query("SELECT name, filename, type, status FROM {system} WHERE type IN ('module', 'theme') ORDER BY filename ASC");
  foreach ($result as $component) {
    // Build directory tree structure.
    $path_parts = explode('/', dirname($component->filename));
    $dir =& $components;
    foreach ($path_parts as $dirname) {
      if (!isset($dir[$dirname])) {
        $dir[$dirname] = array();
      }
      $dir =& $dir[$dirname];
    }

    // Information about components in this directory.
    $component->basename = basename($component->filename);
    $dir['#'. $component->basename] = $component;
    $dir['#-count'] = isset($dir['#-count']) ? $dir['#-count'] + 1 : 1;
  }

  return $components;
}
