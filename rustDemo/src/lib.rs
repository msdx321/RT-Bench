use std::ffi::c_void;
use std::os::raw::c_int;
use std::time::Instant;
use futures::future::join_all;
use tokio::runtime::Runtime;

static mut WORKLOAD_STATE: Option<WorkloadState> = None;

struct WorkloadState {
    data_buffer: Vec<i32>,
    iteration_count: u64,
    start_time: Option<Instant>,
    runtime: Runtime,
}

impl WorkloadState {
    fn new(size: usize) -> Self {
        let runtime = Runtime::new().expect("Failed to create Tokio runtime");
        Self {
            data_buffer: vec![0; size],
            iteration_count: 0,
            start_time: None,
            runtime,
        }
    }
}

async fn task1(task_id: u32) -> u32 {
    let size = 450;
    let matrix_a: Vec<i32> = (0..size*size).map(|i| (i % 100) as i32 + 1).collect();
    let matrix_b: Vec<i32> = (0..size*size).map(|i| (i % 50) as i32 + 1).collect();
    let mut result: Vec<i32> = vec![0; size*size];
    
    for i in 0..size {
        for j in 0..size {
            let mut sum = 0i32;
            for k in 0..size {
                sum = sum.wrapping_add(
                    matrix_a[i * size + k].wrapping_mul(matrix_b[k * size + j])
                );
            }
            result[i * size + j] = sum;
        }
        
        if i % 10 == 0 {
            tokio::task::yield_now().await;
        }
    }
    
    println!("Task {task_id} (450x450) completed");
    result.iter().map(|&x| x as u32).sum::<u32>() % 1000000
}

async fn task2(task_id: u32) -> u32 {
    let size = 600;
    let matrix_a: Vec<i32> = (0..size*size).map(|i| (i % 120) as i32 + 1).collect();
    let matrix_b: Vec<i32> = (0..size*size).map(|i| (i % 80) as i32 + 1).collect();
    let mut result: Vec<i32> = vec![0; size*size];
    
    for i in 0..size {
        for j in 0..size {
            let mut sum = 0i32;
            for k in 0..size {
                sum = sum.wrapping_add(
                    matrix_a[i * size + k].wrapping_mul(matrix_b[k * size + j])
                );
            }
            result[i * size + j] = sum;
        }
        
        if i % 15 == 0 {
            tokio::task::yield_now().await;
        }
    }
    
    println!("Task {task_id} (600x600) completed");
    result.iter().map(|&x| x as u32).sum::<u32>() % 1000000
}

async fn task3(task_id: u32) -> u32 {
    let size = 720;
    let matrix_a: Vec<i32> = (0..size*size).map(|i| (i % 150) as i32 + 1).collect();
    let matrix_b: Vec<i32> = (0..size*size).map(|i| (i % 90) as i32 + 1).collect();
    let mut result: Vec<i32> = vec![0; size*size];
    
    for i in 0..size {
        for j in 0..size {
            let mut sum = 0i32;
            for k in 0..size {
                sum = sum.wrapping_add(
                    matrix_a[i * size + k].wrapping_mul(matrix_b[k * size + j])
                );
            }
            result[i * size + j] = sum;
        }
        
        if i % 20 == 0 {
            tokio::task::yield_now().await;
        }
    }
    
    println!("Task {task_id} (720x720) completed");
    result.iter().map(|&x| x as u32).sum::<u32>() % 1000000
}

async fn task4(task_id: u32) -> u32 {
    let size = 825;
    let matrix_a: Vec<i32> = (0..size*size).map(|i| (i % 180) as i32 + 1).collect();
    let matrix_b: Vec<i32> = (0..size*size).map(|i| (i % 100) as i32 + 1).collect();
    let mut result: Vec<i32> = vec![0; size*size];
    
    for i in 0..size {
        for j in 0..size {
            let mut sum = 0i32;
            for k in 0..size {
                sum = sum.wrapping_add(
                    matrix_a[i * size + k].wrapping_mul(matrix_b[k * size + j])
                );
            }
            result[i * size + j] = sum;
        }
        
        if i % 25 == 0 {
            tokio::task::yield_now().await;
        }
    }
    
    println!("Task {task_id} (825x825) completed");
    result.iter().map(|&x| x as u32).sum::<u32>() % 1000000
}

async fn task5(task_id: u32) -> u32 {
    let size = 930;
    let matrix_a: Vec<i32> = (0..size*size).map(|i| (i % 200) as i32 + 1).collect();
    let matrix_b: Vec<i32> = (0..size*size).map(|i| (i % 110) as i32 + 1).collect();
    let mut result: Vec<i32> = vec![0; size*size];
    
    for i in 0..size {
        for j in 0..size {
            let mut sum = 0i32;
            for k in 0..size {
                sum = sum.wrapping_add(
                    matrix_a[i * size + k].wrapping_mul(matrix_b[k * size + j])
                );
            }
            result[i * size + j] = sum;
        }
        
        if i % 30 == 0 {
            tokio::task::yield_now().await;
        }
    }
    
    println!("Task {task_id} (930x930) completed");
    result.iter().map(|&x| x as u32).sum::<u32>() % 1000000
}

#[no_mangle]
pub unsafe extern "C" fn benchmark_init(parameters_num: c_int, parameters: *mut *mut c_void) -> c_int {
    println!("Rust benchmark_init called with {parameters_num} parameters");
    
    let state = WorkloadState::new(1024);
    WORKLOAD_STATE = Some(state);
    
    println!("Rust benchmark initialized");
    0
}

#[no_mangle]
pub unsafe extern "C" fn benchmark_execution(parameters_num: c_int, parameters: *mut *mut c_void) {
    if let Some(ref mut state) = WORKLOAD_STATE {
        if state.start_time.is_none() {
            state.start_time = Some(Instant::now());
        }
        
        let tasks_to_run = 5 - (state.iteration_count % 5) as usize;
        
        println!("Iteration {}: Running {} async tasks", state.iteration_count, tasks_to_run);
        
        let results = state.runtime.block_on(async {
            let mut tasks = Vec::new();
            
            for i in 0..tasks_to_run {
                let task_id = i as u32;
                
                match i % 5 {
                    0 => {
                        tasks.push(tokio::spawn(task1(task_id)));
                    },
                    1 => {
                        tasks.push(tokio::spawn(task2(task_id)));
                    },
                    2 => {
                        tasks.push(tokio::spawn(task3(task_id)));
                    },
                    3 => {
                        tasks.push(tokio::spawn(task4(task_id)));
                    },
                    4 => {
                        tasks.push(tokio::spawn(task5(task_id)));
                    },
                    _ => unreachable!(),
                }
            }
            
            let results = join_all(tasks).await;
            results.into_iter().map(|r| r.unwrap_or(0)).collect::<Vec<u32>>()
        });
        
        let total_result: u32 = results.iter().sum();
        println!("Iteration {} completed: {} tasks finished with total result: {}", 
                state.iteration_count, tasks_to_run, total_result);

        state.iteration_count += 1;
    } else {
        println!("Warning: benchmark_execution called before benchmark_init");
    }
}

#[no_mangle]
pub unsafe extern "C" fn benchmark_teardown(parameters_num: c_int, _parameters: *mut *mut c_void) {
    println!("Rust benchmark_teardown called with {parameters_num} parameters");
    
    if let Some(ref state) = WORKLOAD_STATE {
        if let Some(start_time) = state.start_time {
            let duration = start_time.elapsed();
            println!("Benchmark completed {} iterations in {:?}", 
                    state.iteration_count, duration);
        }
    }
    
    WORKLOAD_STATE = None;
    println!("Rust benchmark teardown completed");
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[tokio::test]
    async fn test_workloads() {
        let result1 = task1(1).await;
        let result2 = task2(2).await;
        let result3 = task3(3).await;
        let result4 = task4(4).await;
        let result5 = task5(5).await;
        
        assert!(result1 > 0);
        assert!(result2 > 0);
        assert!(result3 > 0);
        assert!(result4 > 0);
        assert!(result5 > 0);
        
        println!("All tasks completed successfully!");
    }
} 