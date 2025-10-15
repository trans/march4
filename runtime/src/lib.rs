// March Runtime - State Management
// Provides persistent/immutable data structures via C FFI

use im::HashMap;
use std::ffi::c_void;

// Opaque state handle for C
#[repr(C)]
pub struct State {
    data: HashMap<u64, u64>,
}

/// Initialize the runtime
#[no_mangle]
pub extern "C" fn march_runtime_init() {
    // Future: any global initialization
}

/// Create a new empty state
#[no_mangle]
pub extern "C" fn march_state_create() -> *mut State {
    let state = Box::new(State {
        data: HashMap::new(),
    });
    Box::into_raw(state)
}

/// Get a value from state (returns 1 if found, 0 if not found)
#[no_mangle]
pub extern "C" fn march_state_get(
    state: *const State,
    key: u64,
    out_value: *mut u64,
) -> i32 {
    if state.is_null() || out_value.is_null() {
        return 0;
    }

    let state = unsafe { &*state };

    if let Some(&value) = state.data.get(&key) {
        unsafe { *out_value = value; }
        1
    } else {
        0
    }
}

/// Set a value in state (returns NEW state, old state unchanged)
#[no_mangle]
pub extern "C" fn march_state_set(
    state: *const State,
    key: u64,
    value: u64,
) -> *mut State {
    if state.is_null() {
        return std::ptr::null_mut();
    }

    let old_state = unsafe { &*state };

    // Create new state with updated value (persistent data structure!)
    let new_data = old_state.data.update(key, value);
    let new_state = Box::new(State { data: new_data });

    Box::into_raw(new_state)
}

/// Free a state (use carefully - only when truly done with it)
#[no_mangle]
pub extern "C" fn march_state_free(state: *mut State) {
    if !state.is_null() {
        unsafe {
            drop(Box::from_raw(state));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_state_operations() {
        let state1 = march_state_create();
        let state2 = march_state_set(state1, 42, 100);

        let mut value: u64 = 0;
        let found = march_state_get(state2, 42, &mut value);

        assert_eq!(found, 1);
        assert_eq!(value, 100);

        // Old state should not have the value
        let found_in_old = march_state_get(state1, 42, &mut value);
        assert_eq!(found_in_old, 0);

        march_state_free(state1);
        march_state_free(state2);
    }
}
