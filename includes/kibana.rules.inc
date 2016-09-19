<?php
/**
 * Rules integration for views statslog.
 */


/**
 * Generate the list of snapshot rows to write.
 */
function _kibana_snapshot_rows($enabled_views) {
  $events = array();

  // Switch user account to root to bypass field access.
  global $user;
  $original_user = $user;
  $old_state = drupal_save_session();
  drupal_save_session(FALSE);
  $user = user_load(1);

  foreach ($enabled_views as $enabled) {
    if (!$enabled) {
      continue;
    }
    list($vid, $did) = explode(':', $enabled);
    $view = views_get_view($vid);

    // Robustness: If the view got deleted for whatever reason skip it to
    // prevent fatal errors.
    if (!$view) {
      continue;
    }
    $view->set_display($did);
    $view->pre_execute();
    $view->execute();

    // Store all value fields and their corresponding aggregation function.
    $value_fields = array();
    // Store all label fields and their corresponding labels.
    $label_fields = array();

    // Everything that has an aggregation function other than "group" is a
    // value field.
    foreach ($view->field as $name => $field) {
      $opts = $field->options;
      if ($opts['group_type'] == 'group') {
        $label_fields[$name] = $opts['label'] ? $opts['label'] : $name;
      }
      else {
        $value_fields[$name] = $opts['group_type'];
      }
    }

    foreach ($view->result as $index => $row) {
      $event = array();
      foreach ($label_fields as $name => $label) {
        $event[$label] = strip_tags($view->render_field($name, $index));
      }

      foreach ($value_fields as $name => $aggregation) {
        $alias = $view->field[$name]->field_alias != 'unknown' ? $view->field[$name]->field_alias : $name;
        $events[] = $event + array(
            'index' => 'snapshot-' . preg_replace('/^(.*?statslog_)/', '', $vid) . '-' . $did,
            'field' => $alias,
            'value' => $row->{$alias},
          );
      }
    }
  }

  // Switch back to original user.
  $user = $original_user;
  drupal_save_session($old_state);

  return $events;
}

/**
 * Execution callback.
 */
function kibana_rules_collect_action($views, $state, $action, $hook) {
  foreach (_kibana_snapshot_rows(array($views)) as $row) {
    kibana_write_object($row['index'], [
      'field' => $row['field'],
      'value' => $row['value'],
    ]);
  }
}

/**
 * Execution callback.
 */
function kibana_rules_log_action($message, $actor, $owners, $settings, $state, $action) {
  $index = 'event-' . preg_replace('/^(.*?statslog_)/', '', $action->root()->name);
  $data = parse_ini_string($message);
  $data['access'] = array('users' => array());

  if ($owners) {
    foreach ($owners as $owner) {
      $data['access']['users'][] = $owner->uid;
    }
  }

  if ($actor && $actor->uid > 0) {
    $data['actor'] = $actor->uid;
  }

  drupal_alter('kibana_access', $data['access'], $account);

  kibana_write_object($index, $data);
}

function kibana_rules_scan_action($tag) {
  $options = array();
  foreach (views_get_all_views() as $vid => $view) {
    $tags = array_map('trim', explode(',', $view->tag));
    if (!in_array($tag, $tags)) {
      continue;
    }
    foreach ($view->display as $did => $display) {
      // Don't use the default view if there are multiple displays.
      if ($did == 'default' && count($view->display) > 1) {
        continue;
      }
      // If display doesn't use aggregation, skip it.
      $aggregation = 0;
      if (isset($view->display['default']->display_options['group_by'])) {
        $aggregation = $view->display['default']->display_options['group_by'];
      }
      if (isset($display->display_options['group_by'])) {
        $aggregation = $display->display_options['group_by'];
      }
      if (!$aggregation) {
        continue;
      }
      $options[] = $vid . ':' . $did;
    }
  }
  return array('views' => $options);
}
