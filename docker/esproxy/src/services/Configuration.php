<?php

namespace Drupal\Kibana\services;

class Configuration {

  protected $ownership_fields = [];

  const DEFAULT_OWNERSHIP_FIELD = 'author';

  public function __construct() {
    $ownership_fields = getenv('OWNERSHIP_FIELDS') ?: static::DEFAULT_OWNERSHIP_FIELD;
    if (file_exists($ownership_fields)) {
      $ownership_fields = file_get_contents($ownership_fields);
    }
    json_decode($ownership_fields);
    
    if (json_last_error() == JSON_ERROR_NONE) {
      $ownership_fields = json_decode($ownership_fields);
    }
    
    $this->ownership_fields = is_array($ownership_fields) ? $ownership_fields : [
      '__default__' => $ownership_fields,
    ];
  }

  public function getDebug() {
    return (bool) getenv('DEBUG');
  }

  public function getTrustedProxies() {
    return explode(',', getenv('TRUSTED_PROXIES') ?: '172.0.0.0/8');
  }

  public function getKibanaIndex() {
    return getenv('KIBANA_INDEX') ?: '.kibana';
  }

  public function getElasticsearchUrl() {
    return getenv('ELASTISEARCH_URL') ?: 'http://elasticsearch:9200';
  }

  public function getSessionDomain() {
    return getenv('SESSION_DOMAIN') ?: '.dork.io';
  }

  public function getDatabaseSettings() {
    return [
      'host' => getenv('MYSQL_HOST'),
      'port' => getenv('MYSQL_PORT'),
      'dbname' => getenv('MYSQL_DATABASE'),
      'user' => getenv('MYSQL_USER'),
      'password' => getenv('MYSQL_PASSWORD'),
    ];
  }

  public function getOwnershipField($index) {
    return array_key_exists($index, $this->ownership_fields) ? $this->ownership_fields[$index] : static::DEFAULT_OWNERSHIP_FIELD;
  }
}
