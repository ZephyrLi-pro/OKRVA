## ADDED Requirements

### Requirement: Agent loop reads estop state
The system SHALL check `EstopManager` state in the agent loop before each tool execution iteration. When `EstopState.kill_all` is `true`, the agent loop SHALL refuse all tool executions and return an error message to the LLM indicating the system is under emergency stop.

#### Scenario: KillAll active
- **WHEN** `EstopState.kill_all` is `true` and the agent loop attempts to execute a tool
- **THEN** the tool execution is skipped and an error message "Emergency stop active (KillAll). All tool execution blocked." is returned as a tool result

#### Scenario: KillAll not active
- **WHEN** `EstopState.kill_all` is `false`
- **THEN** tool execution proceeds normally

### Requirement: Tool execution checks network block
The system SHALL check `EstopState.network_blocked` before executing `http_request` and `curl` tools. When network is blocked, these tools SHALL be refused.

#### Scenario: Network blocked, http_request tool
- **WHEN** `EstopState.network_blocked` is `true` and an `http_request` tool is called
- **THEN** the tool execution is skipped and "Emergency stop active (network blocked). Network tools blocked." is returned

#### Scenario: Network blocked, shell tool with curl
- **WHEN** `EstopState.network_blocked` is `true` and a `shell` tool command contains `curl` or `wget`
- **THEN** the tool execution is skipped and "Emergency stop active (network blocked). Network commands blocked." is returned

#### Scenario: Network not blocked
- **WHEN** `EstopState.network_blocked` is `false`
- **THEN** network-dependent tools execute normally

### Requirement: Tool execution checks frozen tools
The system SHALL check `EstopState.frozen_tools` before executing any tool. When the tool name matches a frozen tool, execution SHALL be refused.

#### Scenario: Tool frozen
- **WHEN** `EstopState.frozen_tools` contains `"shell"` and a `shell` tool is called
- **THEN** the tool execution is skipped and "Emergency stop active (tool frozen: shell)." is returned

#### Scenario: Tool not frozen
- **WHEN** `EstopState.frozen_tools` does not contain the tool name
- **THEN** the tool executes normally

### Requirement: Estop check gated by sub-switch
The estop checks SHALL only execute when `security.estop_enabled` is `true`. When `false`, estop state is not read and all tools execute regardless of estop state.

#### Scenario: Estop integration disabled
- **WHEN** `security.estop_enabled` is `false` and `EstopState.kill_all` is `true`
- **THEN** tools execute normally (estop state is ignored)

#### Scenario: Estop integration enabled
- **WHEN** `security.estop_enabled` is `true` and `EstopState.kill_all` is `true`
- **THEN** all tool executions are blocked

### Requirement: Domain block integration for http_request
The system SHALL check `EstopState.blocked_domains` in the `http_request` tool. When the request URL matches a blocked domain pattern, the request SHALL be refused.

#### Scenario: Domain blocked
- **WHEN** `EstopState.blocked_domains` contains `"*.chase.com"` and an `http_request` targets `https://api.chase.com/...`
- **THEN** the request is refused with "Emergency stop active (domain blocked: *.chase.com)."

#### Scenario: Domain not blocked
- **WHEN** `EstopState.blocked_domains` does not match the request URL
- **THEN** the request executes normally

### Requirement: Estop state read is non-blocking
The system SHALL read estop state via a non-blocking method. If the estop state file is locked or unreadable, the system SHALL assume the fail-closed state (all tools blocked) and log a warning.

#### Scenario: Estop state file unreadable
- **WHEN** `EstopState::load()` fails
- **THEN** the system assumes `EstopState::fail_closed()` and blocks all tool execution
- **AND** logs a warning "Failed to read estop state, assuming fail-closed"

#### Scenario: Estop state file readable
- **WHEN** `EstopState::load()` succeeds
- **THEN** the system uses the loaded state for checks
