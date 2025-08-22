import {
  Chart,
  LineController,
  TimeScale,
  LinearScale,
  PointElement,
  LineElement,
  Legend,
  Filler,
  CategoryScale,
} from 'chart.js';
import 'chartjs-adapter-dayjs-4/dist/chartjs-adapter-dayjs-4.esm';
Chart.register(LineController);
Chart.register(TimeScale);
Chart.register(LinearScale);
Chart.register(CategoryScale);
Chart.register(PointElement);
Chart.register(LineElement);
Chart.register(Filler);
Chart.register(Legend);

import { ApiServiceContext, machine } from '../../services/ApiService.js';
import { useCallback, useEffect, useState, useContext } from 'preact/hooks';
import { computed } from '@preact/signals';
import { Spinner } from '../../components/Spinner.jsx';
import { parseHistoryData } from './utils.js';
import HistoryCard from './HistoryCard.jsx';

const connected = computed(() => machine.value.connected);

// Helper function to ensure numeric values are valid
const ensureValidNumber = (value, fallback = 0) => {
  const num = Number(value);
  return isNaN(num) || !isFinite(num) ? fallback : num;
};

export function ShotHistory() {
  const apiService = useContext(ApiServiceContext);
  const [history, setHistory] = useState([]);
  const [loading, setLoading] = useState(true);
  const [loadingMore, setLoadingMore] = useState(false);
  const [hasMore, setHasMore] = useState(false);
  const [total, setTotal] = useState(0);
  const [offset, setOffset] = useState(0);
  const LIMIT = 5;

  const loadHistory = useCallback(async (reset = false) => {
    const currentOffset = reset ? 0 : ensureValidNumber(offset, 0);
    
    if (reset) {
      setLoading(true);
      setHistory([]);
      setOffset(0);
    } else {
      setLoadingMore(true);
    }

    try {
      const response = await apiService.request({ 
        tp: 'req:history:list',
        offset: currentOffset,
        limit: LIMIT
      });
      
      // Validate response data
      const responseHistory = Array.isArray(response.history) ? response.history : [];
      const newHistory = responseHistory
        .map(parseHistoryData)
        .filter(e => !!e);
      
      if (reset) {
        setHistory(newHistory);
      } else {
        setHistory(prev => [...prev, ...newHistory]);
      }
      
      // Ensure all numeric values are valid
      const responseTotal = ensureValidNumber(response.total, 0);
      const responseHasMore = Boolean(response.hasMore);
      
      setTotal(responseTotal);
      setHasMore(responseHasMore);
      setOffset(currentOffset + LIMIT);
      
      console.log('History loaded:', {
        offset: currentOffset,
        limit: LIMIT,
        total: responseTotal,
        hasMore: responseHasMore,
        historyCount: newHistory.length
      });
      
    } catch (error) {
      console.error('Failed to load history:', error);
      // Reset to safe values on error
      setTotal(0);
      setHasMore(false);
      if (reset) {
        setHistory([]);
        setOffset(0);
      }
    } finally {
      setLoading(false);
      setLoadingMore(false);
    }
  }, [apiService, offset]); // Remove offset from deps to prevent infinite loops

  useEffect(() => {
    if (connected.value) {
      loadHistory(true);
    }
  }, [connected.value]); // Remove loadHistory from deps

  const onDelete = useCallback(
    async id => {
      setLoadingMore(true);
      try {
        await apiService.request({ tp: 'req:history:delete', id });
        // Reload from the beginning after deletion
        setOffset(0); // Reset offset before reloading
        await loadHistory(true);
      } catch (error) {
        console.error('Failed to delete shot:', error);
        setLoadingMore(false);
      }
    },
    [apiService, loadHistory],
  );

  const loadMore = useCallback(() => {
    if (!loadingMore && hasMore && !isNaN(offset)) {
      loadHistory(false);
    }
  }, [loadingMore, hasMore, offset]); // Don't include loadHistory in deps

  // Add validation for render guards
  const safeTotal = ensureValidNumber(total, 0);
  const safeHistoryLength = ensureValidNumber(history.length, 0);
  const safeOffset = ensureValidNumber(offset, 0);

  if (loading) {
    return (
      <div className='flex w-full flex-row items-center justify-center py-16'>
        <Spinner size={8} />
      </div>
    );
  }

  return (
    <>
      <div className='mb-4 flex flex-row items-center gap-2'>
        <h2 className='flex-grow text-2xl font-bold sm:text-3xl'>Shot History</h2>
        <div className='text-sm text-gray-500'>
          Showing {safeHistoryLength} of {safeTotal} shots
        </div>
      </div>

      <div className='grid grid-cols-1 gap-4 lg:grid-cols-12'>
        {history.map((item, idx) => (
          <HistoryCard shot={item} key={item.id || idx} onDelete={id => onDelete(id)} />
        ))}
        
        {history.length === 0 && !loading && (
          <div className='flex flex-row items-center justify-center py-20 lg:col-span-12'>
            <span>No shots available</span>
          </div>
        )}
        
        {hasMore && safeTotal > safeHistoryLength && (
          <div className='flex flex-row items-center justify-center py-8 lg:col-span-12'>
            <button
              onClick={loadMore}
              disabled={loadingMore}
              className='btn btn-primary'
            >
              {loadingMore ? (
                <>
                  <Spinner size={4} />
                  Loading...
                </>
              ) : (
                `Load More (${Math.max(0, safeTotal - safeHistoryLength)} remaining)`
              )}
            </button>
          </div>
        )}
        
        {loadingMore && !hasMore && (
          <div className='flex flex-row items-center justify-center py-8 lg:col-span-12'>
            <Spinner size={6} />
          </div>
        )}
      </div>
    </>
  );
}