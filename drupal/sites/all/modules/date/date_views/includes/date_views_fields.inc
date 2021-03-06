<?php
/**
 * @file
 * Helper for identifying Date API fields for views.
 */

/**
 *  Identify all potential date/timestamp fields.
 *
 *  @return
 *    array with fieldname, type, and table.
 *  @see 
 *    date_views_date_views_fields() which implements 
 *    the hook_date_views_fields() for the core date fields.
 */
function _date_views_fields($base = 'node') {

  // Make sure $base is never empty.
  if (empty($base)) {
    $base = 'node';
  }

  $cid = 'date_views_fields_' . $base;
  cache_clear_all($cid, 'cache_views');

  // We use fields that provide filter handlers as our universe of possible
  // fields of interest.
  $all_fields = date_views_views_fetch_fields($base, 'filter');
  
  // Iterate over all the fields that Views knows about.
  $fields = array();
  foreach ((array) $all_fields as $alias => $val) {

    // Set up some default values.
    $granularity = array('year', 'month', 'day', 'hour', 'minute');
    $tz_handling = 'site';
    $related_fields = array();
    $timezone_field = '';
    $offset_field = '';
    $rrule_field = '';
    $delta_field = '';
    $sql_type = DATE_UNIX;
    $type = '';

    $name = $alias;
    $tmp = explode('.', $name);
    $field_name = $tmp[1];
    $table_name = $tmp[0];

    $fromto = array(str_replace('.', '_', $name), str_replace('.', '_', $name));

    // If we don't have a filter handler, we don't need to do anything more.
    if (!$handler = views_get_handler($table_name, $field_name, 'filter')) {
      continue; 
    }
    $handler = views_get_handler($table_name, $field_name, 'filter');
    $handler_name = $handler->definition['handler'];

    // For Field module fields, get the date type.
    $custom = array();
// http://drupal.org/node/1103790#comment-4306414
//    if ($handler->definition['group'] == 'Fields') {
    if (isset($handler->definition['field_name'])) {
      $field = field_info_field($handler->definition['field_name']);
      switch ($field['type']) {
       case 'date':
          $type = 'cck_string';
          $sql_type = DATE_ISO;
          break;
        case 'datestamp':
          $type = 'cck_timestamp';
          break;
        case 'datetime':
          $type = 'cck_datetime';
          $sql_type = DATE_DATETIME;
          break;          
      }      
      if (empty($type)) {
        // If this is not a date field, nothing more to do.
        continue;
      }
      
      $db_info = date_api_database_info($field);      
      $name = $table_name . "." . $field_name . "_value";
      $granularity = !empty($field['granularity']) ? $field['granularity'] : array('year', 'month', 'day', 'hour', 'minute');

      $fromto = array(
        $table_name . '_' . $db_info['columns'][$table_name]['value'],
        $table_name . '_' . (!empty($field['settings']['todate']) ? $db_info['columns'][$table_name]['value2'] : $db_info['columns'][$table_name]['value']),
      );

      if (isset($field['settings']['tz_handling'])) {
        $tz_handling = $field['settings']['tz_handling'];
        $db_info = date_api_database_info($field);
        if ($tz_handling == 'date') {
          $offset_field = $table_name . '.' . $db_info['columns'][$table_name]['offset'];
        }
        $related_fields = array(
          $table_name . '.' . $db_info['columns'][$table_name]['value']
        );
        if (isset($db_info['columns'][$table_name]['value2'])) {
          $related_fields = array_merge($related_fields, array($table_name . '.' . $db_info['columns'][$table_name]['value2']));
        }
        if (isset($db_info['columns'][$table_name]['timezone'])) {
          $related_fields = array_merge($related_fields, array($table_name . '.' . $db_info['columns'][$table_name]['timezone']));
          $timezone_field = $table_name . '.' . $db_info['columns'][$table_name]['timezone'];
        }
        if (isset($db_info['columns'][$table_name]['rrule'])) {
          $related_fields = array_merge($related_fields, array($table_name . '.' . $db_info['columns'][$table_name]['rrule']));
          $rrule_field = $table_name . '.' . $db_info['columns'][$table_name]['rrule'];
        }
      }
      // Get the delta value into the query.
      if ($field['cardinality'] != 1) {
        array_push($related_fields, "$table_name.delta");
        $delta_field = $table_name . '_delta';
      }      
    }

    // Allow custom modules to provide date fields.
    // The instanceof function makes this work for any handler that was derived 
    // from 'views_handler_filter_date' or 'views_handler_argument_date', 
    // which includes core date fields like the node updated field.
    elseif ($handler instanceof views_handler_filter_date || $handler instanceof views_handler_argument_date) {
      foreach (module_implements('date_views_fields') as $module) {
        $function = $module . '_date_views_fields';
        if ($custom = $function("$table_name.$field_name")) {
          $type = 'custom';
          break;
        }
      }
    }
    // Don't do anything if this is not a date field we can handle.
    if (!empty($type)) {
      $alias = str_replace('.', '_', $alias);
      $fields['name'][$name] = array(
        'type' => $type,
        'sql_type' => $sql_type,
        'label' => $val['group'] . ': ' . $val['title'],
        'granularity' => $granularity,
        'fullname' => $name,
        'table_name' => $table_name,
        'field_name' => $field_name,
        'query_name' => $alias,
        'fromto' => $fromto,
        'tz_handling' => $tz_handling,
        'offset_field' => $offset_field,
        'timezone_field' => $timezone_field,
        'rrule_field' => $rrule_field,
        'related_fields' => $related_fields,
        'delta_field' => $delta_field,
      );

      // Allow the custom fields to over-write values.
      if (!empty($custom)) {
        foreach ($custom as $key => $val) {
          $fields['name'][$name][$key] = $val;
        }
      }
      $fields['name'][$name]['real_field_name'] = $field_name;
      $fields['alias'][$alias] = $fields['name'][$name];
    }
  }
  //cache_set($cid, $fields, 'cache_views');
  return $fields;
}
