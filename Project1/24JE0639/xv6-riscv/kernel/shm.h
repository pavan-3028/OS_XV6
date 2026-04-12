#define shm_max 3 

struct shm_page {
      int key;
      void* pa;
      int programs_attached;
};

struct shm_table {
      struct shm_page pages[shm_max];
};

extern struct shm_table shm_table;
extern int shm_count;
