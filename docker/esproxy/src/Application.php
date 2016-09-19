<?php

namespace Drupal\Kibana;

use Drupal\Kibana\services\User;
use Drupal\Kibana\services\Configuration;
use Drupal\Kibana\services\Proxy;
use Silex\Provider\DoctrineServiceProvider;
use Silex\Provider\ServiceControllerServiceProvider;
use Symfony\Component\HttpFoundation\Request;

class Application extends \Silex\Application {
  public function __construct() {
    parent::__construct();
    $config = new Configuration();
    $this['debug'] = (bool) getenv('DEBUG');

    $this['kibana.config'] = function () use ($config) {
      return $config;
    };
    Request::setTrustedProxies(['172.0.0.0/8']);

    $this['kibana.user'] = function ($app) {
      return new User($this['db'], $this['kibana.config']);
    };

    $this['kibana.proxy'] = function ($app) {
      return new Proxy($this['kibana.config'], $this['kibana.user']);
    };

    $this->register(new ServiceControllerServiceProvider());
    $this->register(new DoctrineServiceProvider(), [
      'db.options' => $config->getDatabaseSettings()
    ]);


    // Endpoints Kibana exposes to the outside.
    // https://github.com/elastic/kibana/blob/5.0/src/core_plugins/elasticsearch/index.js#L54-L59

    // Users have to be able to mget from the kibana index.
    $this->post('/_mget', 'kibana.proxy:multiGet')
      ->before('kibana.user:assertUser');

    // And run personalized mutlisearches.
    $this->post('/_msearch', 'kibana.proxy:multiSearch')
      ->before('kibana.user:assertUser');

    // Get anything out ouf the kibana index.
    $this->get('/.kibana/{url}', 'kibana.proxy:pipe')
      ->assert('url', '.*')
      ->before('kibana.user:assertUser');

    // And run searches against the kibana index.
    $this->post('/.kibana/{type}/_search', 'kibana.proxy:pipe')
      ->before('kibana.user:assertUser');

    // Catch everything else with admin permissions.
    $this->match('{url}', 'kibana.proxy:pipe')->assert('url', '.*')
      ->before('kibana.user:assertAdmin');
  }
}