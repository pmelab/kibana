<?php

namespace Drupal\Kibana\services;

use Doctrine\DBAL\Connection;
use Symfony\Component\HttpFoundation\File\Exception\AccessDeniedException;
use Symfony\Component\HttpFoundation\Request;

class User {
  protected $db;
  protected $config;
  public function __construct(Connection $db, Configuration $config) {
    $this->config = $config;
    $this->db = $db;
  }

  protected function session(Request $request) {
    return ($request->isSecure() ? 'SSESS' : 'SESS') . substr(hash('sha256', $this->config->getSessionDomain()), 0, 32);
  }

  const PERMISSION_ADMIN = 'administer kibana';
  const PERMISSION_USER = 'access kibana';


  function isInternal(Request $request) {
    // TODO: Do a real check. Kibana 5 beta 1 will have custom headers.
    return count($request->cookies) == 0;
  }


  public function getUid(Request $request) {
    $query = "SELECT uid FROM sessions WHERE sid = ?";
    return intval($this->db->fetchColumn($query, [$request->cookies->get($this->session($request))]));
  }

  public function getPermissions(Request $request) {
    $params = [
      $this->getUid($request),
      [static::PERMISSION_ADMIN, static::PERMISSION_USER],
    ];
    $query = "SELECT permission FROM role_permission, users_roles WHERE users_roles.uid = ? AND role_permission.rid = users_roles.rid AND role_permission.permission IN (?)";
    $result = $this->db->fetchAll($query, $params, [\PDO::PARAM_INT, Connection::PARAM_INT_ARRAY]);
    return array_map(function ($row) { return $row['permission']; }, $result);
  }

  function isAdmin(Request $request) {
    return $this->hasPermission($request, static::PERMISSION_ADMIN);
  }

  function isUser(Request $request) {
    return $this->hasPermission($request, static::PERMISSION_USER);
  }

  function assertAdmin(Request $request) {
    $this->assertPermission($request, static::PERMISSION_ADMIN);
  }

  function assertUser(Request $request) {
    $this->assertPermission($request, static::PERMISSION_USER);
  }

  protected function assertPermission(Request $request, $permission) {
    if ($this->isInternal($request)) {
      return;
    }
    if (!$this->hasPermission($request, $permission)) {
      throw new AccessDeniedException($request->getPathInfo());
    }
  }

  protected function hasPermission(Request $request, $permission) {
    return in_array($permission, $this->getPermissions($request));
  }
}
