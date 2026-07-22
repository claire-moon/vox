// SPDX-License-Identifier: GPL-3.0-or-later
//! Modern host boundary. Authoritative simulation remains in the C ABI.

/// ABI version shared with the native headers.
pub const ABI_VERSION: u32 = 8;

/// A small, dependency-free host capability record for the foundation slice.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct HostCapabilities {
    pub abi_version: u32,
    pub worker_threads: u16,
    pub gpu_renderer_available: bool,
}

impl HostCapabilities {
    pub fn foundation(worker_threads: u16) -> Self {
        Self {
            abi_version: ABI_VERSION,
            worker_threads,
            gpu_renderer_available: false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn foundation_capabilities_are_versioned() {
        assert_eq!(HostCapabilities::foundation(2).abi_version, ABI_VERSION);
        assert_eq!(HostCapabilities::foundation(2).worker_threads, 2);
    }
}
